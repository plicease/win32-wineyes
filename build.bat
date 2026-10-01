@echo off
rem Build wineyes.exe with the MSVC toolchain (cl).
setlocal
where cl >nul 2>nul
if not errorlevel 1 goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%%VSWHERE%%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo Could not find Visual Studio with the C++ tools installed.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
:build
cl /nologo /O2 /W4 /EHsc /DUNICODE /D_UNICODE wineyes.cpp /link /SUBSYSTEM:WINDOWS
