@echo off
setlocal enabledelayedexpansion

:: ============================================================
:: FLiNG Downloader Build Script (Ninja)
::
:: Usage: build.cmd [command] [options]
::
:: Commands:
::   release    - Build Release (default)
::   debug      - Build Debug
::   configure  - Configure only (default: release)
::   rebuild    - Clean + rebuild (default: release)
::   clean      - Remove build directory
::   run        - Build and run (default: release)
::   help       - Show help
::
:: Options:
::   --jobs <N>   Parallel build jobs (default: auto)
::
:: Examples:
::   build.cmd              Build Release
::   build.cmd debug        Build Debug
::   build.cmd release      Build Release
::   build.cmd rebuild      Clean + rebuild Release
::   build.cmd rebuild debug  Clean + rebuild Debug
::   build.cmd run debug    Build Debug and launch
::   build.cmd clean        Remove build directory
:: ============================================================

set "SOURCE_DIR=%~dp0"
set "BUILD_DIR=%SOURCE_DIR%build"

:: Defaults
set "COMMAND=build"
set "BUILD_CONFIG=release"
set "JOBS="

:: Parse arguments
:parse_args
if "%~1"=="" goto :done_args

if /i "%~1"=="release"   ( set "BUILD_CONFIG=release" & shift & goto :parse_args )
if /i "%~1"=="debug"     ( set "BUILD_CONFIG=debug"   & shift & goto :parse_args )
if /i "%~1"=="configure" ( set "COMMAND=configure"     & shift & goto :parse_args )
if /i "%~1"=="rebuild"   ( set "COMMAND=rebuild"        & shift & goto :parse_args )
if /i "%~1"=="clean"     ( set "COMMAND=clean"          & shift & goto :parse_args )
if /i "%~1"=="run"       ( set "COMMAND=run"            & shift & goto :parse_args )
if /i "%~1"=="help"      ( goto :show_help )
if /i "%~1"=="-h"        ( goto :show_help )
if /i "%~1"=="--help"    ( goto :show_help )

if /i "%~1"=="--jobs" (
    set "JOBS=%~2"
    shift & shift
    goto :parse_args
)

echo [ERROR] Unknown argument: %~1
echo Run "build.cmd help" for usage.
exit /b 1

:done_args

:: Map config to CMake preset names
if /i "%BUILD_CONFIG%"=="debug"   ( set "CONFIGURE_PRESET=ninja-debug"   & set "BUILD_PRESET=ninja-debug"   & set "CONFIG_LABEL=Debug" )
if /i "%BUILD_CONFIG%"=="release" ( set "CONFIGURE_PRESET=ninja-release" & set "BUILD_PRESET=ninja-release" & set "CONFIG_LABEL=Release" )

:: ============================================================
:: Validate environment
:: ============================================================
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] cmake not found in PATH.
    echo Please install CMake or add it to your PATH.
    exit /b 1
)

where ninja >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] ninja not found in PATH.
    echo Please install Ninja or add it to your PATH.
    exit /b 1
)

if not defined VCPKG_ROOT (
    echo [WARNING] VCPKG_ROOT environment variable is not set.
    echo vcpkg integration may fail.
)

:: ============================================================
:: Execute command
:: ============================================================
if /i "%COMMAND%"=="clean"     goto :do_clean
if /i "%COMMAND%"=="configure" goto :do_configure
if /i "%COMMAND%"=="build"     goto :do_build
if /i "%COMMAND%"=="rebuild"   goto :do_rebuild
if /i "%COMMAND%"=="run"       goto :do_run
goto :eof

:: ------------------------------------------------------------
:do_clean
:: ------------------------------------------------------------
echo.
echo ========================================
echo  Cleaning build directory...
echo ========================================
if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    echo [OK] Build directory removed.
) else (
    echo [OK] Build directory does not exist, nothing to clean.
)
exit /b 0

:: ------------------------------------------------------------
:do_configure
:: ------------------------------------------------------------
echo.
echo ========================================
echo  Configuring [%CONFIG_LABEL%]
echo ========================================
cmake --preset "%CONFIGURE_PRESET%"
if %errorlevel% neq 0 (
    echo [ERROR] CMake configure failed!
    exit /b 1
)
echo [OK] Configure complete.
exit /b 0

:: ------------------------------------------------------------
:do_build
:: ------------------------------------------------------------
:: Auto-configure if build directory doesn't have CMakeCache
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [INFO] No CMakeCache found, running configure first...
    call :do_configure
    if %errorlevel% neq 0 exit /b 1
)

echo.
echo ========================================
echo  Building [%CONFIG_LABEL%]
echo ========================================

set "BUILD_CMD=cmake --build --preset "%BUILD_PRESET%""
if defined JOBS set "BUILD_CMD=%BUILD_CMD% --parallel %JOBS%"

%BUILD_CMD%
if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    exit /b 1
)
echo.
echo [OK] Build complete.
echo [OUTPUT] %BUILD_DIR%\FLiNG Downloader.exe
exit /b 0

:: ------------------------------------------------------------
:do_rebuild
:: ------------------------------------------------------------
call :do_clean
call :do_configure
if %errorlevel% neq 0 exit /b 1
call :do_build
exit /b %errorlevel%

:: ------------------------------------------------------------
:do_run
:: ------------------------------------------------------------
call :do_build
if %errorlevel% neq 0 exit /b 1

echo.
echo ========================================
echo  Launching FLiNG Downloader...
echo ========================================

set "EXE_PATH=%BUILD_DIR%\FLiNG Downloader.exe"

if not exist "!EXE_PATH!" (
    echo [ERROR] Executable not found: !EXE_PATH!
    exit /b 1
)

start "" "!EXE_PATH!"
exit /b 0

:: ------------------------------------------------------------
:show_help
:: ------------------------------------------------------------
echo.
echo  FLiNG Downloader Build Script (Ninja)
echo  ======================================
echo.
echo  Usage: build.cmd [command] [config]
echo.
echo  Commands:
echo    release     Build Release (default)
echo    debug       Build Debug
echo    configure   Configure only
echo    rebuild     Clean, configure, and build
echo    clean       Remove build directory
echo    run         Build and launch the executable
echo    help        Show this help message
echo.
echo  Options:
echo    --jobs ^<N^>  Parallel build jobs
echo.
echo  Examples:
echo    build.cmd                Build Release
echo    build.cmd debug          Build Debug
echo    build.cmd rebuild        Clean + rebuild Release
echo    build.cmd rebuild debug  Clean + rebuild Debug
echo    build.cmd run debug      Build Debug ^& run
echo    build.cmd clean          Remove build directory
echo.
exit /b 0
