/*
 * Amigos cercanos por Bluetooth LE (NimBLE), sin conexiones ni emparejar.
 *
 * Cada reloj emite su "baliza" (23 bytes de pet.h) como datos de fabricante de un
 * anuncio no conectable, y escucha los anuncios de los demás. Con eso basta para
 * verse, invitarse y jugar: el motor resuelve la visita igual en ambos relojes.
 * Alcance típico: unos 10 a 30 metros.
 */
#include <string.h>

#include "hal.h"

#if !MASCOTA_BLUETOOTH
/* Apagado en el prototipo (ver MASCOTA_BLUETOOTH en core/pet.h): el reloj no emite ni escucha nada. El código
 * de abajo queda tal cual para encenderlo más adelante. */
esp_err_t hal_radio_init(void) { return ESP_OK; }
void hal_radio_set_beacon(const uint8_t *datos, int n) { (void)datos; (void)n; }
bool hal_radio_poll(uint8_t out[PET_BALIZA_BYTES]) { (void)out; return false; }
#else

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "hal.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "radio";

#define ID_FABRICANTE 0xFFFF /* reservado para pruebas; sin dueño comercial */
#define ANUNCIO_MS 300
#define ESCUCHA_CADA_MS 320
#define ESCUCHA_VENTANA_MS 50

typedef struct {
    uint8_t d[PET_BALIZA_BYTES];
} baliza_msg_t;

static QueueHandle_t s_rx;
static uint8_t s_tx[2 + PET_BALIZA_BYTES];
static bool s_tx_lista, s_sincronizado;
static uint8_t s_tipo_dir;

static int gap_cb(struct ble_gap_event *ev, void *arg)
{
    (void)arg;
    if (ev->type != BLE_GAP_EVENT_DISC) return 0;
    struct ble_hs_adv_fields f;
    if (ble_hs_adv_parse_fields(&f, ev->disc.data, ev->disc.length_data) != 0) return 0;
    if (!f.mfg_data || f.mfg_data_len != 2 + PET_BALIZA_BYTES) return 0;
    if (f.mfg_data[0] != (ID_FABRICANTE & 0xFF) || f.mfg_data[1] != (ID_FABRICANTE >> 8)) return 0;
    if (f.mfg_data[2] != PET_BALIZA_MAGIC) return 0;
    baliza_msg_t m;
    memcpy(m.d, f.mfg_data + 2, PET_BALIZA_BYTES);
    xQueueSend(s_rx, &m, 0); /* si la cola está llena se pierde: llegará otra en 300 ms */
    return 0;
}

static void anunciar(void)
{
    if (!s_sincronizado || !s_tx_lista) return;
    ble_gap_adv_stop();
    struct ble_hs_adv_fields f = {0};
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.mfg_data = s_tx;
    f.mfg_data_len = sizeof s_tx;
    int rc = ble_gap_adv_set_fields(&f);
    if (rc) { ESP_LOGW(TAG, "adv_set_fields %d", rc); return; }
    struct ble_gap_adv_params p = {0};
    p.conn_mode = BLE_GAP_CONN_MODE_NON;
    p.disc_mode = BLE_GAP_DISC_MODE_NON;
    p.itvl_min = BLE_GAP_ADV_ITVL_MS(ANUNCIO_MS);
    p.itvl_max = BLE_GAP_ADV_ITVL_MS(ANUNCIO_MS + 50);
    rc = ble_gap_adv_start(s_tipo_dir, NULL, BLE_HS_FOREVER, &p, gap_cb, NULL);
    if (rc) ESP_LOGW(TAG, "adv_start %d", rc);
}

static void escuchar(void)
{
    struct ble_gap_disc_params d = {0};
    d.passive = 1;
    d.filter_duplicates = 0; /* queremos cada baliza: su contenido cambia */
    d.itvl = BLE_GAP_SCAN_ITVL_MS(ESCUCHA_CADA_MS);
    d.window = BLE_GAP_SCAN_WIN_MS(ESCUCHA_VENTANA_MS);
    int rc = ble_gap_disc(s_tipo_dir, BLE_HS_FOREVER, &d, gap_cb, NULL);
    if (rc) ESP_LOGW(TAG, "disc %d", rc);
}

static void on_sync(void)
{
    ble_hs_id_infer_auto(0, &s_tipo_dir);
    s_sincronizado = true;
    escuchar();
    anunciar();
    ESP_LOGI(TAG, "Bluetooth listo: buscando amigos");
}

static void on_reset(int motivo) { ESP_LOGW(TAG, "reinicio del host BLE (%d)", motivo); s_sincronizado = false; }

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t hal_radio_init(void)
{
    s_rx = xQueueCreate(12, sizeof(baliza_msg_t));
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "nimble_port_init: %s", esp_err_to_name(err)); return err; }
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_svc_gap_device_name_set("mascota");
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

void hal_radio_set_beacon(const uint8_t *datos, int n)
{
    if (n != PET_BALIZA_BYTES) return;
    if (s_tx_lista && memcmp(s_tx + 2, datos, (size_t)n) == 0) return; /* sin cambios */
    s_tx[0] = ID_FABRICANTE & 0xFF;
    s_tx[1] = ID_FABRICANTE >> 8;
    memcpy(s_tx + 2, datos, (size_t)n);
    s_tx_lista = true;
    anunciar();
}

bool hal_radio_poll(uint8_t out[PET_BALIZA_BYTES])
{
    baliza_msg_t m;
    if (!s_rx || xQueueReceive(s_rx, &m, 0) != pdTRUE) return false;
    memcpy(out, m.d, PET_BALIZA_BYTES);
    return true;
}

#endif /* MASCOTA_BLUETOOTH */
