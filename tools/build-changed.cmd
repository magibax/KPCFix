@echo off
rem Runs tools\build-changed.sh with the sh of Git for Windows. Arguments are passed through.
setlocal
set "SH=%ProgramFiles%\Git\bin\sh.exe"
if not exist "%SH%" (echo Git for Windows sh.exe not found at "%SH%" & exit /b 1)
"%SH%" "%~dp0build-changed.sh" %*
exit /b %ERRORLEVEL%
