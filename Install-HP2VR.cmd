@echo off
echo This launch allows HP2VR scripts for this PowerShell process only.
echo No saved Windows execution policy or antivirus settings are changed.
if not "%~1"=="" goto cli
powershell.exe -ExecutionPolicy Bypass -NoProfile -STA -File "%~dp0Setup-HP2VR.ps1"
goto end
:cli
powershell.exe -ExecutionPolicy Bypass -NoProfile -STA -File "%~dp0Install-HP2VR.ps1" %*
:end
pause
