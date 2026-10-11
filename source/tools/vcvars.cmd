@echo off
rem Usage: call tools\vcvars.cmd x86^|x64
rem Finds any Visual Studio 2022 or later edition with the C++ tools.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio with C++ tools was not found. See BUILDING.md. 1>&2& exit /b 1)
set "VSROOT="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (echo Visual Studio C++ tools were not found. See BUILDING.md. 1>&2& exit /b 1)
if /i "%~1"=="x64" (call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat") else (call "%VSROOT%\VC\Auxiliary\Build\vcvars32.bat")
