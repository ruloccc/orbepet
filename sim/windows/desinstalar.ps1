# Quita el simulador y sus accesos directos. La mascota guardada vive en Edge;
# para borrarla tambien, usa "Nueva mascota" dentro del simulador antes de desinstalar.
$destino = Join-Path $env:LOCALAPPDATA 'MascotaSim'
$escritorio = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Mascota virtual.lnk'
$menu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Mascota virtual.lnk'
foreach ($r in @($escritorio, $menu, $destino)) {
    if (Test-Path $r) { Remove-Item $r -Recurse -Force }
}
Write-Host "Simulador desinstalado."
