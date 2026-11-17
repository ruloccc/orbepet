import json,re,collections
r=json.load(open('/tmp/textos/crudo.json'))
# --- filtrado ---
EXCL_F={'art.h','sfx.h','sfx.c','hal.h','ui.h','foto.h','foto.c','musica.inc','pet.h','services.c','radio.c','art.c'}
EXCL_CALL={'printf','hal_log','strcmp','sscanf','ESP_LOGW','ESP_LOGI','ESP_LOGE','strncmp','hal_evento','strstr','probar_info','hal_storage_set','hal_storage_get','fopen','memcpy','_Static_assert','ESP_RETURN_ON_ERROR','ESP_RETURN_ON_FALSE','xTaskCreatePinnedToCore','xTaskCreate','hal_ram_log','memcmp','spiffs_limpiar_pasos','descarga_log','sonido_anotar','hal_res_save','hal_res_read','hal_res_delete','hal_photo_delete','esp_log_level_set','sizeof','descarga_pantalla','descarga_fin','resumen_linea','sim_texto','lv_obj_set_style_border_color'}
EXCL_FN={'ui.c':{'PRUEBA_NOMBRE','ui_test_panel','revisar_obj','ui_dbg_obj_nombre','toque_log_cb'},
 'pet.c':{'-','nombre_feo','pet_make_name','pet_mode_name','pet_rhythm_name','pet_action_name','pet_activity_name','pet_ambient_name','pet_cause_name','pet_color_name','pet_decor_name','pet_emotion_name','pet_encounter_name','pet_fauna_name','pet_find_name','pet_food_name','pet_form_name','pet_game_name','pet_gesture_name','pet_item_name','pet_mood_name','pet_need_name','pet_neighbor_activity_name','pet_place_name','pet_season_name','pet_stage_name','pet_trait_name','pet_want_name','pet_weather_name','pet_humor_name_x'}}
items=[]
for x in r:
    b=x['f'].split('/')[-1]; x['b']=b
    fn=x['ctx'].split('/')[0]; tb=x['ctx'].split('/')[-1]; x['fn']=fn; x['tb']=tb
    t=x['t']
    if b in EXCL_F or x['call'] in EXCL_CALL: continue
    if b=='main.c' and x['call']!='ui_splash' and not (x['call']=='' and 'ui_splash' in open('/home/claude/src/'+x['f']).read().split('\n')[x['l']-1]): continue
    if fn in EXCL_FN.get(b,()) or tb in EXCL_FN.get(b,()): continue
    if not re.search(r'[A-Za-zÁÉÍÓÚÑáéíóúñ¡¿]{2}',t): continue
    if re.fullmatch(r'[A-Z]{20,}[a-z0-9+/]*',t): continue   # alfabetos de teclado
    if re.fullmatch(r'[\w./%-]+',t) and ('_' in t or '.' in t or '/' in t): continue
    if re.fullmatch(r'[a-z0-9_]+',t) and len(t)<4: continue
    items.append(x)
json.dump(items,open('/tmp/textos/items.json','w'),ensure_ascii=False)
print(len(items))
