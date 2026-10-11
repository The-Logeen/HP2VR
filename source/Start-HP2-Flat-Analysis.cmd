@echo off
setlocal

set "HP2VR_PROJECT=%~dp0"
set "HP2VR_RUNTIME=%HP2VR_PROJECT%runtime"
set "HP2VR_SYSTEM=%HP2VR_RUNTIME%\System"
set "HP2VR_HOST=%HP2VR_PROJECT%build\HP2VRHost.exe"

if not exist "%HP2VR_RUNTIME%\.hp2vr-private-runtime" (
  echo Private HP2VR runtime marker is missing.
  pause
  exit /b 2
)

if not exist "%HP2VR_SYSTEM%\Game.exe" (
  echo Private HP2 Game.exe is missing.
  pause
  exit /b 3
)

if not exist "%HP2VR_HOST%" (
  echo HP2VR safety host is missing.
  pause
  exit /b 4
)

"%HP2VR_HOST%" "%HP2VR_RUNTIME%" "Startup.unr -windowed INI=Game.ini USERINI=User.ini LOG=HP2FlatAnalysis.log"
if errorlevel 1 pause

endlocal
