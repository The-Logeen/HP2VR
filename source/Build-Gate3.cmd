@echo off
setlocal
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

call "%ROOT%\Build-Gate2.cmd"
if errorlevel 1 exit /b 3

set "BUILD=%ROOT%\build"
if not defined HP2VR_HP2_SDK (echo Set HP2VR_HP2_SDK to your sdk_hp2 folder. See BUILDING.md.& exit /b 91)
set "SDK=%HP2VR_HP2_SDK%"
call "%~dp0tools\vcvars.cmd" x86 >nul
if errorlevel 1 exit /b 4

echo === Dedicated HP2 x86 UE engine package ===
cl /nologo /EHsc /std:c++20 "%ROOT%\src\HP2RobeMotionTests.cpp" /Fe"%BUILD%\HP2RobeMotionTests.exe" /Fo"%BUILD%\HP2RobeMotionTests.obj" || exit /b 19
"%BUILD%\HP2RobeMotionTests.exe" || exit /b 20
cl /nologo /EHsc /std:c++20 "%ROOT%\src\HP2GnomeThrowTests.cpp" /Fe"%BUILD%\HP2GnomeThrowTests.exe" /Fo"%BUILD%\HP2GnomeThrowTests.obj" || exit /b 17
"%BUILD%\HP2GnomeThrowTests.exe" || exit /b 18
cl /nologo /EHsc /std:c++20 "%ROOT%\src\HP2ComfortFlickTests.cpp" /Fe"%BUILD%\HP2ComfortFlickTests.exe" /Fo"%BUILD%\HP2ComfortFlickTests.obj" || exit /b 15
"%BUILD%\HP2ComfortFlickTests.exe" || exit /b 16
cl /nologo /EHsc /std:c++20 "%ROOT%\src\HP2ClimbPolicyTests.cpp" /Fe"%BUILD%\HP2ClimbPolicyTests.exe" /Fo"%BUILD%\HP2ClimbPolicyTests.obj" || exit /b 13
"%BUILD%\HP2ClimbPolicyTests.exe" || exit /b 14
cl /nologo /EHsc /std:c++20 "%ROOT%\src\v31-reference\WandInteractionTests.cpp" /Fe"%BUILD%\HP2WandInteractionTests.exe" /Fo"%BUILD%\HP2WandInteractionTests.obj" || exit /b 11
"%BUILD%\HP2WandInteractionTests.exe" || exit /b 12
cl /nologo /O2 /LD /EHsc /std:c++20 /Zp4 /Zc:forScope- /Zc:wchar_t- /D_UNICODE /DUNICODE /DWIN32 /D_WINDOWS /DWINDOWS_IGNORE_PACKING_MISMATCH /DNOMINMAX ^
 /I"%ROOT%\src" /I"%SDK%\Core\Inc" /I"%SDK%\Engine\Inc" /I"%SDK%\Render\Inc" ^
 "%ROOT%\src\HP2VREngine.cpp" /Fe"%BUILD%\HP2VREngine.dll" /Fo"%BUILD%\HP2VREngine.obj" ^
 /link /IMPLIB:"%BUILD%\HP2VRCurrent.lib" /LIBPATH:"%SDK%\Core\Lib" /LIBPATH:"%SDK%\Engine\Lib" /LIBPATH:"%SDK%\Render\Lib" Core.lib Engine.lib Render.lib user32.lib gdi32.lib
if errorlevel 1 exit /b 5

dumpbin /nologo /exports "%BUILD%\HP2VREngine.dll" > "%BUILD%\HP2VREngine.exports.txt"
if errorlevel 1 exit /b 6
findstr /c:" GPackage" "%BUILD%\HP2VREngine.exports.txt" >nul || exit /b 7
findstr /c:" _DllMain@12" "%BUILD%\HP2VREngine.exports.txt" >nul || exit /b 8
findstr /c:" autoclassUHP2VRRender" "%BUILD%\HP2VREngine.exports.txt" >nul || exit /b 9
findstr /c:" autoclassUHP2VRGameEngine" "%BUILD%\HP2VREngine.exports.txt" >nul || exit /b 10

echo Gate 3 engine integration build complete. Runtime configuration was not changed.
exit /b 0
