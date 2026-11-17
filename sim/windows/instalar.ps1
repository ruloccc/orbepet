# Instala el simulador de la mascota como app de escritorio en Windows.
# Copia la pagina a %LOCALAPPDATA%\MascotaSim y crea accesos directos que la abren
# en una ventana propia de Microsoft Edge (modo app), sin barra de direcciones.
$ErrorActionPreference = 'Stop'

$origen = Join-Path $PSScriptRoot 'simulador.html'
if (-not (Test-Path $origen)) { throw "No encuentro simulador.html junto a este instalador." }

$destino = Join-Path $env:LOCALAPPDATA 'MascotaSim'
New-Item -ItemType Directory -Force -Path $destino | Out-Null
$html = Join-Path $destino 'simulador.html'
Copy-Item $origen $html -Force
$url = ([Uri]$html).AbsoluteUri

# Buscar Edge (viene con Windows 10 y 11)
$edge = $null
foreach ($clave in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe',
                     'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe')) {
    try { $edge = (Get-ItemProperty $clave -ErrorAction Stop).'(default)' } catch {}
    if ($edge -and (Test-Path $edge)) { break } else { $edge = $null }
}
if (-not $edge) {
    foreach ($ruta in @("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
                        "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe")) {
        if (Test-Path $ruta) { $edge = $ruta; break }
    }
}

$shell = New-Object -ComObject WScript.Shell
function Crear-Acceso([string]$ruta) {
    $s = $shell.CreateShortcut($ruta)
    if ($edge) {
        $s.TargetPath = $edge
        $s.Arguments = "--app=`"$url`" --window-size=1180,900"
        $s.IconLocation = "$edge,0"
    } else {
        $s.TargetPath = $html   # sin Edge: se abre en el navegador predeterminado
    }
    $s.Description = 'Simulador de la mascota virtual'
    $s.WorkingDirectory = $destino
    $s.Save()
}

$escritorio = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Mascota virtual.lnk'
$menu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Mascota virtual.lnk'
Crear-Acceso $escritorio
Crear-Acceso $menu

Write-Host ""
Write-Host "Listo. Quedo instalado en $destino"
Write-Host "Accesos directos: escritorio y menu Inicio ('Mascota virtual')."
Start-Process $escritorio
