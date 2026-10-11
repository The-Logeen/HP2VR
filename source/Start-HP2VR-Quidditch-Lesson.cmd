@echo off
powershell.exe -NoProfile -File "%~dp0tools\Start-HP2VR-Quidditch.ps1"
if errorlevel 1 pause
