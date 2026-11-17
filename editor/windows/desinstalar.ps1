# Quita el editor y sus accesos directos. Tus dibujos guardados viven en Edge; si quieres
# conservarlos, usa "Guardar paquete" antes de desinstalar.
$destino = Join-Path $env:LOCALAPPDATA 'MascotaEditor'
$escritorio = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Editor de la mascota.lnk'
$menu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Editor de la mascota.lnk'
foreach ($r in @($escritorio, $menu, $destino)) {
    if (Test-Path $r) { Remove-Item $r -Recurse -Force }
}
Write-Host "Editor desinstalado."
