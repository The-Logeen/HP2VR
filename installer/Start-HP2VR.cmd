@echo off
if exist "%~dp0INSTALL-INCOMPLETE.txt" (
 echo Installation is incomplete. Do not launch this copy.
 pause
 exit /b 1
)
powershell.exe -NoProfile -File "%~dp0tools\Start-HP2VR.ps1"
if errorlevel 1 pause
