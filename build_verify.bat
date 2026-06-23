@echo off
rem ============================================================================
rem  build_verify.bat - compile and run the headless Physarum core check.
rem
rem  No Visual Studio IDE needed. The script locates the MSVC toolchain with
rem  vswhere, sets up the x64 environment, builds verify.cpp + physarum.cpp,
rem  and runs the result. Output goes to bin\verify\.
rem ============================================================================
setlocal

rem --- Already inside a Developer Command Prompt? Skip the vcvars dance. ---
where cl >nul 2>nul
if %errorlevel%==0 goto :build

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [build_verify] vswhere not found. Open a "x64 Native Tools Command
    echo                Prompt for VS 2022" and run this script again.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo [build_verify] No MSVC C++ toolset found via vswhere.
    exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if %errorlevel% neq 0 (
    echo [build_verify] Failed to initialise the x64 build environment.
    exit /b 1
)

:build
set "OUTDIR=bin\verify"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo [build_verify] Compiling (Release, C++20)...
cl /nologo /std:c++20 /O2 /EHsc /MD /DNDEBUG /DNOMINMAX /I src ^
   src\verify.cpp src\physarum.cpp ^
   /Fo"%OUTDIR%\\" /Fe"%OUTDIR%\PhysarumVerify.exe"
if %errorlevel% neq 0 (
    echo [build_verify] Build failed.
    exit /b 1
)

echo.
echo [build_verify] Running...
echo.
"%OUTDIR%\PhysarumVerify.exe"
endlocal
