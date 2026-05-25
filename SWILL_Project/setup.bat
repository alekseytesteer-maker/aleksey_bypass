@echo off
setlocal enabledelayedexpansion
echo ==================================================
echo SWILL Project Setup - Downloading MinHook
echo ==================================================

set MINHOOK_URL=https://github.com/TsudaKageyu/minhook/releases/download/v1.3.3/MinHook-v1.3.3.zip
set MINHOOK_ZIP=%TEMP%\minhook_v133.zip
set MINHOOK_DIR=%~dp03rdparty\MinHook
set EXTRACT_DIR=%~dp03rdparty\minhook_tmp

if exist "%MINHOOK_DIR%\include\MinHook.h" (
    echo [OK] MinHook already installed.
    goto :BUILD
)

echo [*] Downloading MinHook v1.3.3...
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { Invoke-WebRequest -Uri '%MINHOOK_URL%' -OutFile '%MINHOOK_ZIP%' -UseBasicParsing; Write-Host 'Download complete' } catch { Write-Host 'Download failed: $_' -ForegroundColor Red; exit 1 }"

if errorlevel 1 (
    echo [!] PowerShell download failed. Trying alternative method...
    certutil -urlcache -split -f "%MINHOOK_URL%" "%MINHOOK_ZIP%" >nul 2>&1
)

if not exist "%MINHOOK_ZIP%" (
    echo [!] Failed to download MinHook.
    echo Please download manually from:
    echo %MINHOOK_URL%
    echo And extract to: %MINHOOK_DIR%
    goto :MANUAL
)

echo [*] Extracting MinHook...
if exist "%EXTRACT_DIR%" rmdir /s /q "%EXTRACT_DIR%"
mkdir "%EXTRACT_DIR%"

powershell -NoProfile -ExecutionPolicy Bypass -Command "try { Expand-Archive -Path '%MINHOOK_ZIP%' -DestinationPath '%EXTRACT_DIR%' -Force; Write-Host 'Extraction complete' } catch { Write-Host 'Extraction failed: $_' -ForegroundColor Red; exit 1 }"

if errorlevel 1 (
    echo [!] PowerShell extraction failed. Trying manual unzip...
    certutil -urlcache -delete "%MINHOOK_URL%" >nul 2>&1
    goto :MANUAL
)

REM Peremeshchaem faily iz vremennoy papki
if exist "%EXTRACT_DIR%\MinHook-v1.3.3\include" (
    xcopy /E /I /Y "%EXTRACT_DIR%\MinHook-v1.3.3\include" "%MINHOOK_DIR%\include"
    xcopy /E /I /Y "%EXTRACT_DIR%\MinHook-v1.3.3\lib" "%MINHOOK_DIR%\lib"
) else if exist "%EXTRACT_DIR%\include" (
    xcopy /E /I /Y "%EXTRACT_DIR%\include" "%MINHOOK_DIR%\include"
    xcopy /E /I /Y "%EXTRACT_DIR%\lib" "%MINHOOK_DIR%\lib"
) else (
    echo [!] Unexpected archive structure.
    goto :MANUAL
)

rmdir /s /q "%EXTRACT_DIR%"
del /Q "%MINHOOK_ZIP%" 2>nul

if exist "%MINHOOK_DIR%\include\MinHook.h" (
    echo [OK] MinHook successfully installed!
    goto :BUILD
)

:MANUAL
echo ==================================================
echo MANUAL INSTALLATION REQUIRED
echo ==================================================
echo Please:
echo 1. Download MinHook from: %MINHOOK_URL%
echo 2. Extract files to: %MINHOOK_DIR%
echo    - include\MinHook.h should exist
echo    - lib\MinHook.lib should exist
echo ==================================================
goto :BUILD

:BUILD
echo ==================================================
echo Checking installation...
if exist "%MINHOOK_DIR%\include\MinHook.h" (
    echo [OK] MinHook headers found
) else (
    echo [!!] MinHook headers NOT found - build will fail
)
if exist "%MINHOOK_DIR%\lib\MinHook.lib" (
    echo [OK] MinHook library found
) else (
    echo [!!] MinHook library NOT found - build will fail
)
echo ==================================================
echo Setup complete. Open SWILL_Project.sln in Visual Studio.
echo Build configuration: Release ^| Win32
echo ==================================================
pause
exit /b 0
