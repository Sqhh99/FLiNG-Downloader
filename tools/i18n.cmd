@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "ROOT_DIR=%%~fI"
set "TRANSLATION_DIR=%ROOT_DIR%\resources\translations"

set "TS_EN=%TRANSLATION_DIR%\flingdownloader_en_US.ts"
set "TS_JA=%TRANSLATION_DIR%\flingdownloader_ja_JP.ts"
set "QM_EN=%TRANSLATION_DIR%\flingdownloader_en_US.qm"
set "QM_JA=%TRANSLATION_DIR%\flingdownloader_ja_JP.qm"

set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=all"

if /i "%ACTION%"=="help" goto :show_help
if /i "%ACTION%"=="-h" goto :show_help
if /i "%ACTION%"=="--help" goto :show_help

if /i "%ACTION%"=="update" goto :do_update
if /i "%ACTION%"=="release" goto :do_release
if /i "%ACTION%"=="all" goto :do_all
if /i "%ACTION%"=="check" goto :do_check

echo [ERROR] Unknown i18n action: %ACTION%
echo Run "tools\i18n.cmd help" for usage.
exit /b 1

:do_update
call :resolve_qt_tool lupdate LUPDATE_EXE
if errorlevel 1 exit /b 1

echo [INFO] Updating TS files...
"%LUPDATE_EXE%" "%ROOT_DIR%\src" "%ROOT_DIR%\qml" -ts "%TS_EN%" "%TS_JA%"
if errorlevel 1 (
    echo [ERROR] lupdate failed.
    exit /b 1
)

echo [OK] TS files updated.
exit /b 0

:do_release
call :resolve_qt_tool lrelease LRELEASE_EXE
if errorlevel 1 exit /b 1

echo [INFO] Generating QM files...
"%LRELEASE_EXE%" "%TS_EN%" -qm "%QM_EN%"
if errorlevel 1 (
    echo [ERROR] lrelease failed for en_US.
    exit /b 1
)

"%LRELEASE_EXE%" "%TS_JA%" -qm "%QM_JA%"
if errorlevel 1 (
    echo [ERROR] lrelease failed for ja_JP.
    exit /b 1
)

echo [OK] QM files generated.
exit /b 0

:do_all
call :do_update
if errorlevel 1 exit /b 1
call :do_release
if errorlevel 1 exit /b 1

echo [OK] i18n update completed.
exit /b 0

:do_check
call "%~f0" all
if errorlevel 1 exit /b 1

git -C "%ROOT_DIR%" diff --exit-code -- "%TRANSLATION_DIR%" >nul
if errorlevel 1 (
    echo [ERROR] Translation files are out of sync.
    echo [ERROR] Run "build.cmd i18n" and commit changes under resources\translations.
    git -C "%ROOT_DIR%" --no-pager diff -- "%TRANSLATION_DIR%"
    exit /b 1
)

echo [OK] Translation files are in sync.
exit /b 0

:resolve_qt_tool
set "TOOL_NAME=%~1"
set "TOOL_VAR=%~2"
set "FOUND_TOOL="

if defined QT_BIN_DIR (
    if exist "%QT_BIN_DIR%\%TOOL_NAME%.exe" (
        set "FOUND_TOOL=%QT_BIN_DIR%\%TOOL_NAME%.exe"
    )
)

if not defined FOUND_TOOL (
    for /f "delims=" %%P in ('where %TOOL_NAME% 2^>nul') do (
        if not defined FOUND_TOOL set "FOUND_TOOL=%%P"
    )
)

if not defined FOUND_TOOL (
    echo [ERROR] %TOOL_NAME%.exe not found.
    echo [ERROR] Set QT_BIN_DIR to your Qt bin directory or add Qt bin to PATH.
    exit /b 1
)

set "%TOOL_VAR%=%FOUND_TOOL%"
exit /b 0

:show_help
echo.
echo  FLiNG Downloader i18n Script
echo  ============================
echo.
echo  Usage: tools\i18n.cmd [action]
echo.
echo  Actions:
echo    update   Update TS files from src/qml strings
echo    release  Build QM files from TS
echo    all      Update TS + build QM ^(default^)
echo    check    Run all, then fail if resources\translations has uncommitted changes
echo    help     Show this help
echo.
echo  Optional environment:
echo    QT_BIN_DIR   Path to Qt bin directory containing lupdate.exe and lrelease.exe
echo.
exit /b 0
