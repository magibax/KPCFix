@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Check-KPCVersion.ps1"
set "RESULT=%ERRORLEVEL%"
echo.
pause
exit /b %RESULT%
