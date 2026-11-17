@echo off
rem Instala el simulador de la mascota virtual (no necesita permisos de administrador)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0instalar.ps1"
if errorlevel 1 pause
