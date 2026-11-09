@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup-KPCFix.ps1" -OnLaunch
if errorlevel 1 (
  pause
  exit /b 1
)
start "" "%~dp0Psycho.exe"
