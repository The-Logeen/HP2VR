@echo off
setlocal
set "ROOT=%~dp0"
if not defined HP2VR_OPENVR_SDK (echo Set HP2VR_OPENVR_SDK to your extracted OpenVR SDK 2.15.6 folder. See BUILDING.md.& exit /b 90)
set "OPENVR=%HP2VR_OPENVR_SDK%"
call "%~dp0tools\vcvars.cmd" x64 >nul
if errorlevel 1 exit /b 1
cl /nologo /O2 /EHsc /std:c++20 /I"%ROOT%src" /I"%OPENVR%\headers" "%ROOT%src\HP2VRCompanion.cpp" /Fe"%ROOT%build\HP2VRCompanion.exe" /Fo"%ROOT%build\HP2VRCompanion.obj" /link d3d11.lib dxgi.lib user32.lib gdi32.lib "%OPENVR%\lib\win64\openvr_api.lib"
if errorlevel 1 exit /b 2
"%ROOT%build\HP2VRCompanion.exe" --offline
exit /b %errorlevel%
