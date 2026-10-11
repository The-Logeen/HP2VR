@echo off
setlocal
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD=%ROOT%\build"
if not exist "%BUILD%" mkdir "%BUILD%"

call "%~dp0tools\vcvars.cmd" x86 >nul
if errorlevel 1 exit /b 3

echo === HP2 bridge and build-identity tests ===
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\tests\HP2VRBridgeTests.cpp" /Fe"%BUILD%\HP2VRBridgeTests.exe" /Fo"%BUILD%\HP2VRBridgeTests.obj" || exit /b 4
"%BUILD%\HP2VRBridgeTests.exe" || exit /b 5
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\tests\HP2BuildIdentityTests.cpp" /Fe"%BUILD%\HP2BuildIdentityTests.exe" /Fo"%BUILD%\HP2BuildIdentityTests.obj" || exit /b 6
"%BUILD%\HP2BuildIdentityTests.exe" || exit /b 7

echo === Read-only HP2 build inspector ===
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\src\HP2VRInspector.cpp" /Fe"%BUILD%\HP2VRInspector.exe" /Fo"%BUILD%\HP2VRInspector.obj" /link advapi32.lib || exit /b 8

echo === Private home-directory redirect ===
cl /nologo /O2 /LD /EHsc /std:c++20 "%ROOT%\src\HP2HomeProxy.cpp" /Fe"%BUILD%\shfolder.dll" /Fo"%BUILD%\HP2HomeProxy.obj" /link /DEF:"%ROOT%\src\HP2HomeProxy.def" /IMPLIB:"%BUILD%\HP2HomeProxy.lib" || exit /b 9
cl /nologo /EHsc /std:c++20 "%ROOT%\tests\HP2HomeProxyTests.cpp" /Fe"%BUILD%\HP2HomeProxyTests.exe" /Fo"%BUILD%\HP2HomeProxyTests.obj" || exit /b 10
"%BUILD%\HP2HomeProxyTests.exe" "%BUILD%\shfolder.dll" || exit /b 11

echo === Gate 1 safety launcher ===
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\tests\HP2ProcessSessionTests.cpp" /Fe"%BUILD%\HP2ProcessSessionTests.exe" /Fo"%BUILD%\HP2ProcessSessionTests.obj" || exit /b 13
"%BUILD%\HP2ProcessSessionTests.exe" || exit /b 14
cl /nologo /EHsc /std:c++20 /I"%ROOT%\src" "%ROOT%\src\HP2VRGate1Launcher.cpp" /Fe"%BUILD%\HP2VRHost.exe" /Fo"%BUILD%\HP2VRGate1Launcher.obj" /link advapi32.lib user32.lib || exit /b 12

echo Gate 1 build complete. Nothing was installed or launched.
exit /b 0

