@echo off
echo ================================================
echo SWILL Project Setup - Downloading MinHook
echo ================================================

cd /d "%~dp0"

if not exist "3rdparty\MinHook\include" mkdir "3rdparty\MinHook\include"
if not exist "3rdparty\MinHook\lib.x86" mkdir "3rdparty\MinHook\lib.x86"

echo [*] Downloading MinHook v1.3.3...

powershell -Command "& {Invoke-WebRequest -Uri 'https://github.com/TsudaKageyu/minhook/releases/download/v1.3.3/MinHook-v1.3.3-bin.zip' -OutFile 'minhook.zip'}"

if exist "minhook.zip" (
    echo [*] Extracting MinHook...
    powershell -Command "& {Expand-Archive -Path 'minhook.zip' -DestinationPath 'temp_minhook' -Force}"
    
    if exist "temp_minhook\MinHook-v1.3.3\include\MinHook.h" (
        copy /Y "temp_minhook\MinHook-v1.3.3\include\MinHook.h" "3rdparty\MinHook\include\" > nul
        echo [+] MinHook.h copied
    )
    
    if exist "temp_minhook\MinHook-v1.3.3\bin\mhook.x86.lib" (
        copy /Y "temp_minhook\MinHook-v1.3.3\bin\mhook.x86.lib" "3rdparty\MinHook\lib.x86\MinHook.lib" > nul
        echo [+] MinHook.lib copied
    )
    
    rmdir /S /Q "temp_minhook"
    del /Q "minhook.zip"
    
    echo ================================================
    echo [+] Setup completed successfully!
    echo ================================================
    echo Now you can open SWILL_Project.sln in Visual Studio
) else (
    echo [!] Failed to download MinHook. Please check your internet connection.
    echo Alternatively, download manually from:
    echo https://github.com/TsudaKageyu/minhook/releases
)

pause
