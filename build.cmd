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
::   tests      - Configure, build, and run tests
::   benchmark  - Configure, build, and run benchmarks
::   configure  - Configure only (default: release)
::   rebuild    - Clean + rebuild (default: release)
::   clean      - Remove build directory
::   i18n       - Update/check translation files
::   run        - Build and run (default: release)
::   help       - Show help
::
:: Options:
::   --jobs <N>   Parallel build jobs (default: auto)
::   --app-version <V>  Override app version (for example: 1.1.5)
::   --filter <PATTERN>  Benchmark filter pattern
::
:: Examples:
::   build.cmd              Build Release
::   build.cmd debug        Build Debug
::   build.cmd release      Build Release
::   build.cmd rebuild      Clean + rebuild Release
::   build.cmd rebuild debug  Clean + rebuild Debug
::   build.cmd i18n         Update TS and QM translation files
::   build.cmd i18n check   Verify translation files are in sync
::   build.cmd run debug    Build Debug and launch
::   build.cmd clean        Remove build directory
::   build.cmd release --app-version 1.1.5
:: ============================================================

set "SOURCE_DIR=%~dp0"
set "BUILD_ROOT=%SOURCE_DIR%build"
set "BUILD_DIR=%BUILD_ROOT%\ninja-release"

:: Defaults
set "COMMAND=build"
set "BUILD_CONFIG=release"
set "I18N_ACTION=all"
set "JOBS="
set "APP_VERSION_OVERRIDE=%FLING_APP_VERSION%"
set "BENCHMARK_FILTER="
set "CONFIGURE_DONE="

:: Parse arguments
:parse_args
if "%~1"=="" goto :done_args

if /i "%~1"=="release"   ( set "BUILD_CONFIG=release" & shift & goto :parse_args )
if /i "%~1"=="debug"     ( set "BUILD_CONFIG=debug"   & shift & goto :parse_args )
if /i "%~1"=="tests"     ( set "COMMAND=tests"         & shift & goto :parse_args )
if /i "%~1"=="benchmark" ( set "COMMAND=benchmark"     & shift & goto :parse_args )
if /i "%~1"=="configure" ( set "COMMAND=configure"     & shift & goto :parse_args )
if /i "%~1"=="rebuild"   ( set "COMMAND=rebuild"        & shift & goto :parse_args )
if /i "%~1"=="clean"     ( set "COMMAND=clean"          & shift & goto :parse_args )
if /i "%~1"=="i18n"      ( set "COMMAND=i18n" & shift & goto :parse_i18n )
if /i "%~1"=="run"       ( set "COMMAND=run"            & shift & goto :parse_args )
if /i "%~1"=="help"      ( goto :show_help )
if /i "%~1"=="-h"        ( goto :show_help )
if /i "%~1"=="--help"    ( goto :show_help )

if /i "%~1"=="--jobs" (
    if "%~2"=="" (
        echo [ERROR] --jobs requires a positive integer value.
        exit /b 1
    )
    echo %~2| findstr /r "^[1-9][0-9]*$" >nul
    if errorlevel 1 (
        echo [ERROR] Invalid --jobs value: %~2
        echo Please provide a positive integer.
        exit /b 1
    )
    set "JOBS=%~2"
    shift & shift
    goto :parse_args
)

if /i "%~1"=="--app-version" (
    if "%~2"=="" (
        echo [ERROR] --app-version requires a value.
        exit /b 1
    )
    set "APP_VERSION_OVERRIDE=%~2"
    shift & shift
    goto :parse_args
)

if /i "%~1"=="--filter" (
    if "%~2"=="" (
        echo [ERROR] --filter requires a value.
        exit /b 1
    )
    set "BENCHMARK_FILTER=%~2"
    shift & shift
    goto :parse_args
)

echo [ERROR] Unknown argument: %~1
echo Run "build.cmd help" for usage.
exit /b 1

:parse_i18n
if "%~1"=="" goto :done_args
if /i "%~1"=="update"  ( set "I18N_ACTION=update"  & shift & goto :parse_i18n )
if /i "%~1"=="release" ( set "I18N_ACTION=release" & shift & goto :parse_i18n )
if /i "%~1"=="all"     ( set "I18N_ACTION=all"     & shift & goto :parse_i18n )
if /i "%~1"=="check"   ( set "I18N_ACTION=check"   & shift & goto :parse_i18n )
if /i "%~1"=="help"    ( set "I18N_ACTION=help"    & shift & goto :parse_i18n )
if /i "%~1"=="-h"      ( set "I18N_ACTION=help"    & shift & goto :parse_i18n )
if /i "%~1"=="--help"  ( set "I18N_ACTION=help"    & shift & goto :parse_i18n )
echo [ERROR] Unknown i18n action: %~1
echo Run "build.cmd i18n help" for usage.
exit /b 1

:done_args

:: Map config to CMake preset names
if /i "%BUILD_CONFIG%"=="debug"   ( set "CONFIGURE_PRESET=debug"   & set "BUILD_PRESET=debug"   & set "CONFIG_LABEL=Debug" & set "BUILD_DIR=%BUILD_ROOT%\ninja-debug" )
if /i "%BUILD_CONFIG%"=="release" ( set "CONFIGURE_PRESET=release" & set "BUILD_PRESET=release" & set "CONFIG_LABEL=Release" & set "BUILD_DIR=%BUILD_ROOT%\ninja-release" )

if /i not "%COMMAND%"=="i18n" (
    if defined APP_VERSION_OVERRIDE (
        powershell -NoProfile -Command "$v=$env:APP_VERSION_OVERRIDE; if ($v -match '^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$') { exit 0 } else { exit 1 }"
        if errorlevel 1 (
            echo [ERROR] Invalid --app-version value.
            echo [ERROR] Expected format: SemVer-style ^(for example 1.2.3, 1.2.3-beta.1, 1.2.3+build.5^).
            echo [ERROR] Received: "%APP_VERSION_OVERRIDE%"
            exit /b 1
        )
    )
)

:: ============================================================
:: Validate environment
:: ============================================================
if /i "%COMMAND%"=="i18n" goto :execute_command

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH.
    echo Please install CMake or add it to your PATH.
    exit /b 1
)

where ninja >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ninja not found in PATH.
    echo Please install Ninja or add it to your PATH.
    exit /b 1
)

if not defined VCPKG_ROOT (
    if exist "%SOURCE_DIR%third_party\vcpkg\scripts\buildsystems\vcpkg.cmake" (
        set "VCPKG_ROOT=%SOURCE_DIR%third_party\vcpkg"
        echo [INFO] VCPKG_ROOT not set, using bundled vcpkg:
        echo        !VCPKG_ROOT!
    ) else if defined VCPKG_INSTALLATION_ROOT (
        set "VCPKG_ROOT=%VCPKG_INSTALLATION_ROOT%"
        echo [INFO] VCPKG_ROOT not set, using VCPKG_INSTALLATION_ROOT:
        echo        !VCPKG_ROOT!
    ) else (
        echo [WARNING] VCPKG_ROOT environment variable is not set.
        echo vcpkg integration may fail.
    )
)

:: ============================================================
:: Execute command
:: ============================================================
:execute_command
if /i "%COMMAND%"=="clean"     goto :do_clean
if /i "%COMMAND%"=="tests"     goto :do_tests
if /i "%COMMAND%"=="benchmark" goto :do_benchmark
if /i "%COMMAND%"=="configure" goto :do_configure
if /i "%COMMAND%"=="build"     goto :do_build
if /i "%COMMAND%"=="rebuild"   goto :do_rebuild
if /i "%COMMAND%"=="i18n"      goto :do_i18n
if /i "%COMMAND%"=="run"       goto :do_run
goto :eof

:: ------------------------------------------------------------
:do_clean
:: ------------------------------------------------------------
echo.
echo ========================================
echo  Cleaning build directory...
echo ========================================
if exist "%BUILD_ROOT%" (
    rmdir /s /q "%BUILD_ROOT%"
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
echo [INFO] Configure preset: %CONFIGURE_PRESET%
echo [INFO] Binary directory: %BUILD_DIR%

:: Self-heal stale generator tool path from old CMake cache.
if exist "%BUILD_DIR%\CMakeCache.txt" (
    set "CACHE_MAKE_PROGRAM="
    for /f "tokens=1,* delims==" %%A in ('findstr /b "CMAKE_MAKE_PROGRAM:FILEPATH=" "%BUILD_DIR%\CMakeCache.txt"') do (
        set "CACHE_MAKE_PROGRAM=%%B"
    )

    if defined CACHE_MAKE_PROGRAM (
        set "CACHE_MAKE_PROGRAM=!CACHE_MAKE_PROGRAM:/=\!"
        if not exist "!CACHE_MAKE_PROGRAM!" (
            echo [WARNING] Cached build tool path is invalid: !CACHE_MAKE_PROGRAM!
            echo [INFO] Removing stale CMake cache from "%BUILD_DIR%"...
            del /f /q "%BUILD_DIR%\CMakeCache.txt" >nul 2>&1
            if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles"
        )
    )
)

if defined APP_VERSION_OVERRIDE (
    echo [INFO] App version override: "%APP_VERSION_OVERRIDE%"
    cmake --preset "%CONFIGURE_PRESET%" -DAPP_VERSION="%APP_VERSION_OVERRIDE%"
) else (
    cmake --preset "%CONFIGURE_PRESET%"
)
if errorlevel 1 (
    echo [ERROR] CMake configure failed!
    exit /b 1
)
set "CONFIGURE_DONE=1"
echo [OK] Configure complete.
exit /b 0

:: ------------------------------------------------------------
:do_build
:: ------------------------------------------------------------
:: Self-heal stale Ninja path in existing cache.
if exist "%BUILD_DIR%\CMakeCache.txt" (
    set "CACHE_MAKE_PROGRAM="
    for /f "tokens=1,* delims==" %%A in ('findstr /b "CMAKE_MAKE_PROGRAM:FILEPATH=" "%BUILD_DIR%\CMakeCache.txt"') do (
        set "CACHE_MAKE_PROGRAM=%%B"
    )

    if defined CACHE_MAKE_PROGRAM (
        set "CACHE_MAKE_PROGRAM=!CACHE_MAKE_PROGRAM:/=\!"
        if not exist "!CACHE_MAKE_PROGRAM!" (
            echo [WARNING] Cached build tool path is invalid: !CACHE_MAKE_PROGRAM!
            echo [INFO] Re-running configure to refresh generator tool path...
            call :do_configure
            if errorlevel 1 exit /b 1
        )
    )
)

:: Auto-configure if needed.
if defined APP_VERSION_OVERRIDE (
    if not defined CONFIGURE_DONE (
        echo [INFO] App version override detected, refreshing configure cache...
        call :do_configure
        if errorlevel 1 exit /b 1
    )
) else (
    if not exist "%BUILD_DIR%\CMakeCache.txt" (
        echo [INFO] No CMakeCache found, running configure first...
        call :do_configure
        if errorlevel 1 exit /b 1
    )
)

echo.
echo ========================================
echo  Building [%CONFIG_LABEL%]
echo ========================================
echo [INFO] Build preset: %BUILD_PRESET%
echo [INFO] Binary directory: %BUILD_DIR%
if defined JOBS (
    echo [INFO] Parallel jobs: %JOBS%
) else (
    echo [INFO] Parallel jobs: auto
)

if defined JOBS (
    cmake --build --preset "%BUILD_PRESET%" --parallel %JOBS%
) else (
    cmake --build --preset "%BUILD_PRESET%"
)
if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
)
echo.
echo [OK] Build complete.
echo [OUTPUT] %BUILD_DIR%\FLiNG Downloader.exe
exit /b 0

:: ------------------------------------------------------------
:do_tests
:: ------------------------------------------------------------
echo.
echo ========================================
echo  Testing [%CONFIG_LABEL%]
echo ========================================
echo [INFO] Configure preset: %CONFIGURE_PRESET%
echo [INFO] Build preset: %BUILD_PRESET%
echo [INFO] Binary directory: %BUILD_DIR%

if defined APP_VERSION_OVERRIDE (
    echo [INFO] App version override: "%APP_VERSION_OVERRIDE%"
    cmake --preset "%CONFIGURE_PRESET%" -DFLING_BUILD_TESTS=ON -DAPP_VERSION="%APP_VERSION_OVERRIDE%"
) else (
    cmake --preset "%CONFIGURE_PRESET%" -DFLING_BUILD_TESTS=ON
)
if errorlevel 1 (
    echo [ERROR] CMake configure for tests failed!
    exit /b 1
)

if defined JOBS (
    cmake --build --preset "%BUILD_PRESET%" --target FLiNGDownloaderTests --parallel %JOBS%
) else (
    cmake --build --preset "%BUILD_PRESET%" --target FLiNGDownloaderTests
)
if errorlevel 1 (
    echo [ERROR] Test build failed!
    exit /b 1
)

ctest --test-dir "%BUILD_DIR%" --output-on-failure
if errorlevel 1 (
    echo [ERROR] Tests failed!
    exit /b 1
)

echo [OK] Tests passed.
exit /b 0

:: ------------------------------------------------------------
:do_benchmark
:: ------------------------------------------------------------
echo.
echo ========================================
echo  Benchmarking [%CONFIG_LABEL%]
echo ========================================
echo [INFO] Configure preset: %CONFIGURE_PRESET%
echo [INFO] Build preset: %BUILD_PRESET%
echo [INFO] Binary directory: %BUILD_DIR%

if defined APP_VERSION_OVERRIDE (
    echo [INFO] App version override: "%APP_VERSION_OVERRIDE%"
    cmake --preset "%CONFIGURE_PRESET%" -DFLING_BUILD_BENCHMARKS=ON -DAPP_VERSION="%APP_VERSION_OVERRIDE%"
) else (
    cmake --preset "%CONFIGURE_PRESET%" -DFLING_BUILD_BENCHMARKS=ON
)
if errorlevel 1 (
    echo [ERROR] CMake configure for benchmarks failed!
    exit /b 1
)

if defined JOBS (
    cmake --build --preset "%BUILD_PRESET%" --target FLiNGDownloaderBenchmarks --parallel %JOBS%
) else (
    cmake --build --preset "%BUILD_PRESET%" --target FLiNGDownloaderBenchmarks
)
if errorlevel 1 (
    echo [ERROR] Benchmark build failed!
    exit /b 1
)

set "BENCHMARK_EXE=%BUILD_DIR%\FLiNG Downloader Benchmarks.exe"
if not exist "%BENCHMARK_EXE%" (
    echo [ERROR] Benchmark executable not found: "%BENCHMARK_EXE%"
    exit /b 1
)

if defined BENCHMARK_FILTER (
    "%BENCHMARK_EXE%" --benchmark_filter=%BENCHMARK_FILTER%
) else (
    "%BENCHMARK_EXE%"
)
if errorlevel 1 (
    echo [ERROR] Benchmarks failed!
    exit /b 1
)

echo [OK] Benchmarks completed.
exit /b 0

:: ------------------------------------------------------------
:do_rebuild
:: ------------------------------------------------------------
call :do_clean
call :do_configure
if errorlevel 1 exit /b 1
call :do_build
exit /b %errorlevel%

:: ------------------------------------------------------------
:do_i18n
:: ------------------------------------------------------------
echo.
echo ========================================
echo  Running i18n [%I18N_ACTION%]
echo ========================================

if not exist "%SOURCE_DIR%tools\i18n.cmd" (
    echo [ERROR] i18n script not found: %SOURCE_DIR%tools\i18n.cmd
    exit /b 1
)

call "%SOURCE_DIR%tools\i18n.cmd" %I18N_ACTION%
if errorlevel 1 (
    echo [ERROR] i18n step failed.
    exit /b 1
)

echo [OK] i18n step complete.
exit /b 0

:: ------------------------------------------------------------
:do_run
:: ------------------------------------------------------------
call :do_build
if errorlevel 1 exit /b 1

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
echo    tests       Configure, build, and run tests
echo    benchmark   Configure, build, and run benchmarks
echo    rebuild     Clean, configure, and build
echo    clean       Remove build directory
echo    i18n        Update/check translation files
echo    run         Build and launch the executable
echo    help        Show this help message
echo.
echo  Options:
echo    --jobs ^<N^>  Parallel build jobs (positive integer)
echo    --app-version ^<V^>  Override app version (e.g. 1.1.5)
echo.
echo  Examples:
echo    build.cmd                Build Release
echo    build.cmd debug          Build Debug
echo    build.cmd rebuild        Clean + rebuild Release
echo    build.cmd rebuild debug  Clean + rebuild Debug
echo    build.cmd tests          Build and run tests
echo    build.cmd tests debug    Build and run Debug tests
echo    build.cmd benchmark      Build and run benchmarks
echo    build.cmd benchmark --filter CoverExtractor/all_images
echo    build.cmd i18n           Update TS and QM translation files
echo    build.cmd i18n check     Verify translation files are in sync
echo    build.cmd run debug      Build Debug ^& run
echo    build.cmd clean          Remove build directory
echo    build.cmd release --app-version 1.1.5
echo.
exit /b 0
