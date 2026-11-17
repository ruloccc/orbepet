@echo off
chcp 65001 >nul
cd /d "%~dp0herramientas"
set PY=
py -3 -c "import tkinter" >nul 2>nul && (set PY=py -3& goto :listo)
python -c "import tkinter" >nul 2>nul && (set PY=python& goto :listo)
for /d %%D in (C:\Espressif\tools\python\*) do if exist "%%D\venv\Scripts\python.exe" "%%D\venv\Scripts\python.exe" -c "import tkinter" >nul 2>nul && (set PY="%%D\venv\Scripts\python.exe"& goto :listo)
for /d %%D in (C:\Espressif\tools\idf-python\*) do if exist "%%D\python.exe" "%%D\python.exe" -c "import tkinter" >nul 2>nul && (set PY="%%D\python.exe"& goto :listo)
echo.
echo No encontre un Python con ventanas (tkinter).
echo Instala Python desde https://www.python.org/downloads/ (deja marcada la opcion "tcl/tk") y vuelve a abrir este archivo.
echo.
pause
exit /b 1
:listo
%PY% mascota_gui.py
if errorlevel 1 pause
