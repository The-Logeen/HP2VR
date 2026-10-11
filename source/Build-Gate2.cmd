@echo off
setlocal
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD=%ROOT%\build"
if not exist "%BUILD%" mkdir "%BUILD%"
copy /y "%ROOT%\assets\WandAtlas.pcx" "%BUILD%\WandAtlas.pcx" >nul
copy /y "%ROOT%\assets\AvatarAtlas.pcx" "%BUILD%\AvatarAtlas.pcx" >nul
if errorlevel 1 exit /b 30

if not "%HP2VR_SKIP_GATE1%"=="1" (
  call "%ROOT%\Build-Gate1.cmd"
  if errorlevel 1 exit /b 3
)

call "%~dp0tools\vcvars.cmd" x86 >nul
if errorlevel 1 exit /b 4

echo === HP2 x86 D3D11 transport proxy ===
cl /nologo /O2 /LD /EHsc /std:c++20 /I"%ROOT%\src" ^
 "%ROOT%\src\HP2D3D11Proxy.cpp" /Fe"%BUILD%\d3d11.dll" /Fo"%BUILD%\HP2D3D11Proxy.obj" ^
 /link /DEF:"%ROOT%\src\HP2D3D11Proxy.def" /IMPLIB:"%BUILD%\HP2D3D11Proxy.lib" dxgi.lib
if errorlevel 1 exit /b 5
cl /nologo /EHsc /std:c++20 "%ROOT%\tests\HP2D3D11ProxyTests.cpp" ^
 /Fe"%BUILD%\HP2D3D11ProxyTests.exe" /Fo"%BUILD%\HP2D3D11ProxyTests.obj"
if errorlevel 1 exit /b 6
"%BUILD%\HP2D3D11ProxyTests.exe" "%BUILD%\d3d11.dll"
if errorlevel 1 exit /b 7
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\tests\HP2StereoProjectionTests.cpp" ^
 /Fe"%BUILD%\HP2StereoProjectionTests.exe" /Fo"%BUILD%\HP2StereoProjectionTests.obj"
if errorlevel 1 exit /b 8
"%BUILD%\HP2StereoProjectionTests.exe"
if errorlevel 1 exit /b 9
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\tests\HP2NativeInputTests.cpp" ^
 /Fe"%BUILD%\HP2NativeInputTests.exe" /Fo"%BUILD%\HP2NativeInputTests.obj"
if errorlevel 1 exit /b 22
"%BUILD%\HP2NativeInputTests.exe"
if errorlevel 1 exit /b 23
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\tests\HP2PlayerFeelTests.cpp" ^
 /Fe"%BUILD%\HP2PlayerFeelTests.exe" /Fo"%BUILD%\HP2PlayerFeelTests.obj"
if errorlevel 1 exit /b 26
"%BUILD%\HP2PlayerFeelTests.exe"
if errorlevel 1 exit /b 27
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\tests\HP2ScreenPanelTests.cpp" ^
 /Fe"%BUILD%\HP2ScreenPanelTests.exe" /Fo"%BUILD%\HP2ScreenPanelTests.obj"
if errorlevel 1 exit /b 28
"%BUILD%\HP2ScreenPanelTests.exe"
if errorlevel 1 exit /b 29
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\tests\HP2LessonQuidditchTests.cpp" ^
 /Fe"%BUILD%\HP2LessonQuidditchTests.exe" /Fo"%BUILD%\HP2LessonQuidditchTests.obj"
if errorlevel 1 exit /b 24
"%BUILD%\HP2LessonQuidditchTests.exe"
if errorlevel 1 exit /b 25

echo === Read-only HP2 package table inspector ===
cl /nologo /EHsc /std:c++20 "%ROOT%\src\HP2PackageInspector.cpp" ^
 /Fe"%BUILD%\HP2PackageInspector.exe" /Fo"%BUILD%\HP2PackageInspector.obj"
if errorlevel 1 exit /b 10

echo === HP2 SDK compile-time layouts ===
if not defined HP2VR_HP2_SDK (echo Set HP2VR_HP2_SDK to your sdk_hp2 folder. See BUILDING.md.& exit /b 91)
set "SDK=%HP2VR_HP2_SDK%"
cl /nologo /c /EHsc /std:c++20 /Zp4 /Zc:wchar_t- /D_UNICODE /DUNICODE /DWIN32 /D_WINDOWS /DWINDOWS_IGNORE_PACKING_MISMATCH ^
 /I"%SDK%\Core\Inc" /I"%SDK%\Engine\Inc" /I"%SDK%\Render\Inc" ^
 "%ROOT%\tests\HP2SdkLayoutTests.cpp" /Fo"%BUILD%\HP2SdkLayoutTests.obj"
if errorlevel 1 exit /b 11

echo === Read-only HP2 live ABI probe ===
cl /nologo /O2 /LD /EHsc /std:c++20 /Zp4 /Zc:wchar_t- /D_UNICODE /DUNICODE /DWIN32 /D_WINDOWS /DWINDOWS_IGNORE_PACKING_MISMATCH /DNOMINMAX ^
 /I"%SDK%\Core\Inc" /I"%SDK%\Engine\Inc" /I"%SDK%\Render\Inc" ^
 "%ROOT%\src\HP2VRProbe.cpp" /Fe"%BUILD%\HP2VRProbe.dll" /Fo"%BUILD%\HP2VRProbe.obj" ^
 /link /DEF:"%ROOT%\src\HP2VRProbe.def" /IMPLIB:"%BUILD%\HP2VRProbe.lib" /LIBPATH:"%SDK%\Core\Lib" /LIBPATH:"%SDK%\Engine\Lib" /LIBPATH:"%SDK%\Render\Lib" Core.lib Engine.lib Render.lib
if errorlevel 1 exit /b 12
dumpbin /nologo /exports "%BUILD%\HP2VRProbe.dll" | findstr /e /c:"HP2VR_InitializeProbe" >nul
if errorlevel 1 exit /b 13

echo === HP2 x64 SteamVR companion ===
if not defined HP2VR_OPENVR_SDK (echo Set HP2VR_OPENVR_SDK to your extracted OpenVR SDK 2.15.6 folder. See BUILDING.md.& exit /b 90)
set "OPENVR=%HP2VR_OPENVR_SDK%"
call "%~dp0tools\vcvars.cmd" x64 >nul
if errorlevel 1 exit /b 14
cl /nologo /O2 /EHsc /std:c++20 /I"%ROOT%\src" /I"%OPENVR%\headers" ^
 "%ROOT%\src\HP2VRCompanion.cpp" /Fe"%BUILD%\HP2VRCompanion.exe" /Fo"%BUILD%\HP2VRCompanion.obj" ^
 /link d3d11.lib dxgi.lib user32.lib gdi32.lib "%OPENVR%\lib\win64\openvr_api.lib"
if errorlevel 1 exit /b 15
copy /y "%OPENVR%\bin\win64\openvr_api.dll" "%BUILD%\openvr_api.dll" >nul
if errorlevel 1 exit /b 16
copy /y "%ROOT%\vr\actions_hp2_v1.json" "%BUILD%\actions_hp2_v1.json" >nul
if errorlevel 1 exit /b 17
copy /y "%ROOT%\vr\bindings_hp2_oculus_touch.json" "%BUILD%\bindings_hp2_oculus_touch.json" >nul
if errorlevel 1 exit /b 18
copy /y "%ROOT%\vr\bindings_hp2_knuckles.json" "%BUILD%\bindings_hp2_knuckles.json" >nul
if errorlevel 1 exit /b 19
copy /y "%ROOT%\vr\bindings_hp2_vive_controller.json" "%BUILD%\bindings_hp2_vive_controller.json" >nul
if errorlevel 1 exit /b 20
"%BUILD%\HP2VRCompanion.exe" --offline
if errorlevel 1 exit /b 21

echo Gate 2 transport build complete. Nothing was installed or launched.
copy /y "%ROOT%\assets\tessellation.cfg" "%BUILD%\tessellation.cfg" >nul
if errorlevel 1 exit /b 22
exit /b 0
