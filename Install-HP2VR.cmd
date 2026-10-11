@echo off
if not "%~1"=="" goto cli
powershell.exe -NoProfile -STA -File "%~dp0Setup-HP2VR.ps1"
goto end
:cli
powershell.exe -NoProfile -STA -File "%~dp0Install-HP2VR.ps1" %*
:end
pause
