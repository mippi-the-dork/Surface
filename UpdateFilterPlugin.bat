@echo off
setlocal EnableExtensions

REM ============================================================
REM FilterPlugin.ini Updater
REM
REM Put this BAT in the root of an Unreal Engine plugin.
REM Running it will overwrite:
REM     <PluginRoot>\Config\FilterPlugin.ini
REM
REM If Config does not exist, it will be created.
REM ============================================================

pushd "%~dp0"

set "PLUGIN_ROOT=%CD%"
set "CONFIG_DIR=%PLUGIN_ROOT%\Config"
set "FILTER_FILE=%CONFIG_DIR%\FilterPlugin.ini"

echo.
echo ============================================================
echo FilterPlugin.ini Updater
echo ============================================================
echo.
echo Plugin Root:
echo %PLUGIN_ROOT%
echo.
echo Target:
echo %FILTER_FILE%
echo.

if not exist "%CONFIG_DIR%\" (
    echo Config folder not found. Creating it...
    mkdir "%CONFIG_DIR%"
    if errorlevel 1 (
        echo.
        echo ERROR: Could not create Config folder.
        echo.
        popd
        pause
        exit /b 1
    )
)

echo Writing standard FilterPlugin.ini...

> "%FILTER_FILE%" echo [FilterPlugin]
>>"%FILTER_FILE%" echo.
>>"%FILTER_FILE%" echo /README.md
>>"%FILTER_FILE%" echo /Doc/...
>>"%FILTER_FILE%" echo.
>>"%FILTER_FILE%" echo -/FAB/...
>>"%FILTER_FILE%" echo -/Release/...
>>"%FILTER_FILE%" echo -/*.bat
>>"%FILTER_FILE%" echo -/Tests/...
>>"%FILTER_FILE%" echo -/.../Tests/...

if errorlevel 1 (
    echo.
    echo ERROR: Failed to write FilterPlugin.ini.
    echo.
    popd
    pause
    exit /b 1
)

echo.
echo ============================================================
echo SUCCESS
echo FilterPlugin.ini has been updated.
echo ============================================================
echo.

popd
pause
exit /b 0
