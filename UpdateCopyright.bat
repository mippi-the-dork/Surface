@echo off
setlocal EnableExtensions

REM ============================================================
REM Copyright Header Updater
REM
REM Put this BAT in the root of an Unreal Engine plugin.
REM It ONLY scans:
REM     <PluginRoot>\Source\
REM ============================================================


REM ============================================================
REM USER SETTINGS
REM ============================================================

REM Exact copyright line to place on line 1.
set "COPYRIGHT_TEXT=// Copyright Mippithedork 2026, Inc. All Rights Reserved."

REM File types to process recursively inside Source.
REM Separate entries with semicolons.
set "FILE_TYPES=*.h;*.cpp;*.cs"


REM ============================================================
REM SCRIPT
REM You normally should not need to edit below this line.
REM ============================================================

pushd "%~dp0"

set "PLUGIN_ROOT=%CD%"
set "SOURCE_DIR=%CD%\Source"
set "PS_SCRIPT=%TEMP%\UpdateCopyright_%RANDOM%_%RANDOM%.ps1"

echo.
echo ============================================================
echo Copyright Header Updater
echo ============================================================
echo.
echo Plugin Root:
echo %PLUGIN_ROOT%
echo.
echo Source Folder:
echo %SOURCE_DIR%
echo.
echo Copyright:
echo %COPYRIGHT_TEXT%
echo.
echo File Types:
echo %FILE_TYPES%
echo.

if not exist "%SOURCE_DIR%\" (
    echo ERROR: No Source folder was found.
    echo.
    echo Expected:
    echo "%SOURCE_DIR%"
    echo.
    popd
    pause
    exit /b 1
)

REM Write the PowerShell worker script to a temporary file.
> "%PS_SCRIPT%" echo $ErrorActionPreference = 'Stop'
>>"%PS_SCRIPT%" echo $root = $env:SOURCE_DIR
>>"%PS_SCRIPT%" echo $copyright = $env:COPYRIGHT_TEXT
>>"%PS_SCRIPT%" echo $patterns = $env:FILE_TYPES -split ';' ^| Where-Object { $_.Trim() -ne '' }
>>"%PS_SCRIPT%" echo $files = @($patterns ^| ForEach-Object { Get-ChildItem -LiteralPath $root -Recurse -File -Filter $_ -ErrorAction Stop } ^| Sort-Object FullName -Unique)
>>"%PS_SCRIPT%" echo $updated = 0
>>"%PS_SCRIPT%" echo $unchanged = 0
>>"%PS_SCRIPT%" echo foreach ($file in $files) {
>>"%PS_SCRIPT%" echo     $reader = [System.IO.StreamReader]::new($file.FullName, $true)
>>"%PS_SCRIPT%" echo     try {
>>"%PS_SCRIPT%" echo         $text = $reader.ReadToEnd()
>>"%PS_SCRIPT%" echo         $encoding = $reader.CurrentEncoding
>>"%PS_SCRIPT%" echo     } finally {
>>"%PS_SCRIPT%" echo         $reader.Dispose()
>>"%PS_SCRIPT%" echo     }
>>"%PS_SCRIPT%" echo.
>>"%PS_SCRIPT%" echo     $newline = if ($text.Contains([char]13 + [char]10)) { [char]13 + [char]10 } else { [char]10 }
>>"%PS_SCRIPT%" echo.
>>"%PS_SCRIPT%" echo     $lines = [System.Collections.Generic.List[string]]::new()
>>"%PS_SCRIPT%" echo     if ($text.Length -gt 0) {
>>"%PS_SCRIPT%" echo         foreach ($line in ($text -split '\r?\n', -1)) {
>>"%PS_SCRIPT%" echo             [void]$lines.Add($line)
>>"%PS_SCRIPT%" echo         }
>>"%PS_SCRIPT%" echo     }
>>"%PS_SCRIPT%" echo.
>>"%PS_SCRIPT%" echo     while ($lines.Count -gt 0 -and [string]::IsNullOrWhiteSpace($lines[0])) {
>>"%PS_SCRIPT%" echo         $lines.RemoveAt(0)
>>"%PS_SCRIPT%" echo     }
>>"%PS_SCRIPT%" echo.
>>"%PS_SCRIPT%" echo     if ($lines.Count -gt 0 -and $lines[0] -match '^\s*//\s*Copyright\b') {
>>"%PS_SCRIPT%" echo         $lines.RemoveAt(0)
>>"%PS_SCRIPT%" echo     }
>>"%PS_SCRIPT%" echo.
>>"%PS_SCRIPT%" echo     while ($lines.Count -gt 0 -and [string]::IsNullOrWhiteSpace($lines[0])) {
>>"%PS_SCRIPT%" echo         $lines.RemoveAt(0)
>>"%PS_SCRIPT%" echo     }
>>"%PS_SCRIPT%" echo.
>>"%PS_SCRIPT%" echo     $body = [string]::Join($newline, $lines)
>>"%PS_SCRIPT%" echo     $newText = $copyright + $newline + $newline + $body
>>"%PS_SCRIPT%" echo.
>>"%PS_SCRIPT%" echo     if ($newText -ne $text) {
>>"%PS_SCRIPT%" echo         $writer = [System.IO.StreamWriter]::new($file.FullName, $false, $encoding)
>>"%PS_SCRIPT%" echo         try {
>>"%PS_SCRIPT%" echo             $writer.Write($newText)
>>"%PS_SCRIPT%" echo         } finally {
>>"%PS_SCRIPT%" echo             $writer.Dispose()
>>"%PS_SCRIPT%" echo         }
>>"%PS_SCRIPT%" echo.
>>"%PS_SCRIPT%" echo         Write-Host ('UPDATED: ' + $file.FullName)
>>"%PS_SCRIPT%" echo         $updated++
>>"%PS_SCRIPT%" echo     } else {
>>"%PS_SCRIPT%" echo         $unchanged++
>>"%PS_SCRIPT%" echo     }
>>"%PS_SCRIPT%" echo }
>>"%PS_SCRIPT%" echo.
>>"%PS_SCRIPT%" echo Write-Host ''
>>"%PS_SCRIPT%" echo Write-Host '============================================================'
>>"%PS_SCRIPT%" echo Write-Host ('Files found: ' + $files.Count)
>>"%PS_SCRIPT%" echo Write-Host ('Files updated: ' + $updated)
>>"%PS_SCRIPT%" echo Write-Host ('Already correct: ' + $unchanged)
>>"%PS_SCRIPT%" echo Write-Host '============================================================'

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%"
set "RESULT=%ERRORLEVEL%"

del /F /Q "%PS_SCRIPT%" >nul 2>&1

if not "%RESULT%"=="0" (
    echo.
    echo ERROR: The copyright update failed.
    echo.
    popd
    pause
    exit /b %RESULT%
)

echo.
echo Copyright update complete.
echo.

popd
pause
exit /b 0
