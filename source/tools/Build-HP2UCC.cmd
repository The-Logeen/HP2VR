@echo off
setlocal
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
if not defined HP2VR_HP2_SDK (echo Set HP2VR_HP2_SDK to your sdk_hp2 folder. See BUILDING.md.& exit /b 91)
set "SDK=%HP2VR_HP2_SDK%"

call "%~dp0..\tools\vcvars.cmd" x86 >nul
if errorlevel 1 exit /b 3

cl /nologo /Zp4 /Zc:forScope- /Zc:wchar_t- /MD /EHsc /O2 ^
 /I"%SDK%\Core\Inc" /I"%SDK%\Engine\Inc" /I"%SDK%\Filer\Inc" /I"%SDK%\UCC\Inc" ^
 /DNDEBUG /D_CONSOLE /DUNICODE /D_UNICODE /DWIN32 /D_WINDOWS /DWINDOWS_IGNORE_PACKING_MISMATCH /DThisPackage=UCC ^
 "%SDK%\UCC\Src\UCC.cpp" "%ROOT%\src\HP2UCCArrayShim.cpp" /Fo"%ROOT%\build\\" /Fe"%ROOT%\build\HP2UCC.exe" ^
 /link "%SDK%\Core\Lib\Core.lib" /subsystem:console /machine:I386
if errorlevel 1 exit /b 4

echo Built the SDK-matched HP2 commandlet host for private, read-only package research.
exit /b 0
