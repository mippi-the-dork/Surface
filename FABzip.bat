@echo off
setlocal EnableExtensions

REM ============================================================
REM FAB Source Package Builder
REM
REM Put this BAT in the root of an Unreal Engine plugin.
REM It creates:
REM     <PluginRoot>\FAB\<PluginFolderName>.zip
REM
REM The ZIP contains ONLY:
REM     Config\
REM     Doc\
REM     Resources\
REM     Source\
REM     README.md
REM     <PluginName>.uplugin
REM
REM Any folder named Tests is excluded recursively.
REM ============================================================

pushd "%~dp0"

for %%I in ("%CD%") do set "PLUGIN_NAME=%%~nxI"

set "FAB_DIR=%CD%\FAB"
set "ZIP_PATH=%FAB_DIR%\%PLUGIN_NAME%.zip"
set "TEMP_DIR=%TEMP%\FABPackage_%PLUGIN_NAME%_%RANDOM%_%RANDOM%"

REM ============================================================
REM VALIDATION
REM ============================================================

if not exist "%FAB_DIR%\" (
    echo Creating FAB folder...
    mkdir "%FAB_DIR%" >nul 2>&1
    if errorlevel 1 (
        echo.
        echo ERROR: Could not create:
        echo "%FAB_DIR%"
        goto :error
    )
)

if not exist "%CD%\README.md" (
    echo.
    echo ERROR: README.md was not found in the plugin root.
    goto :error
)

set "UPLUGIN_FILE="
for %%F in ("%CD%\*.uplugin") do (
    if not defined UPLUGIN_FILE set "UPLUGIN_FILE=%%~fF"
)

if not defined UPLUGIN_FILE (
    echo.
    echo ERROR: No .uplugin file was found in the plugin root.
    goto :error
)

REM ============================================================
REM PREPARE OUTPUT
REM ============================================================

echo.
echo ============================================================
echo FAB Source Package Builder
echo ============================================================
echo.
echo Plugin: %PLUGIN_NAME%
echo Output:
echo "%ZIP_PATH%"
echo.

if exist "%ZIP_PATH%" (
    echo Deleting previous FAB ZIP...
    del /F /Q "%ZIP_PATH%"
    if exist "%ZIP_PATH%" (
        echo.
        echo ERROR: Could not delete the previous ZIP.
        echo Make sure it is not open in another application.
        goto :error
    )
)

if exist "%TEMP_DIR%\" rmdir /S /Q "%TEMP_DIR%" >nul 2>&1
mkdir "%TEMP_DIR%" >nul 2>&1
if errorlevel 1 (
    echo.
    echo ERROR: Could not create temporary staging folder:
    echo "%TEMP_DIR%"
    goto :error
)

REM ============================================================
REM COPY WHITELISTED CONTENT
REM ============================================================

echo Building FAB package contents...

for %%D in (Config Doc Resources Source) do (
    if exist "%CD%\%%D\" (
        robocopy "%CD%\%%D" "%TEMP_DIR%\%%D" /E /R:0 /W:0 /NFL /NDL /NJH /NJS /NP /XD "Tests" >nul
        if errorlevel 8 (
            echo.
            echo ERROR: Failed while copying %%D.
            goto :cleanup_error
        )
    )
)

copy /Y "%CD%\README.md" "%TEMP_DIR%\README.md" >nul
if errorlevel 1 (
    echo.
    echo ERROR: Failed to copy README.md.
    goto :cleanup_error
)

for %%F in ("%UPLUGIN_FILE%") do set "UPLUGIN_NAME=%%~nxF"
copy /Y "%UPLUGIN_FILE%" "%TEMP_DIR%\%UPLUGIN_NAME%" >nul
if errorlevel 1 (
    echo.
    echo ERROR: Failed to copy %UPLUGIN_NAME%.
    goto :cleanup_error
)

REM ============================================================
REM CREATE ZIP
REM ============================================================

echo Creating ZIP...

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%TEMP_DIR%\*' -DestinationPath '%ZIP_PATH%' -CompressionLevel Optimal -Force"
if errorlevel 1 (
    echo.
    echo ERROR: ZIP creation failed.
    goto :cleanup_error
)

rmdir /S /Q "%TEMP_DIR%" >nul 2>&1

echo.
echo ============================================================
echo SUCCESS
echo Created:
echo "%ZIP_PATH%"
echo ============================================================
echo.

popd
pause
exit /b 0

:cleanup_error
if exist "%TEMP_DIR%\" rmdir /S /Q "%TEMP_DIR%" >nul 2>&1

:error
echo.
echo FAB packaging failed.
echo.
popd
pause
exit /b 1
