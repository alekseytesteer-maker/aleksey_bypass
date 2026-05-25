@echo off
chcp 65001 > nul
echo ============================================================
echo          SWILL Project - Автоматическая настройка
echo ============================================================
echo.

REM Проверка наличия PowerShell
where powershell >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [!] Ошибка: PowerShell не найден!
    pause
    exit /b 1
)

echo [*] Загрузка MinHook...
echo.

REM Создаем директорию для MinHook
if not exist "SWILL_Payload\MinHook" mkdir "SWILL_Payload\MinHook"
if not exist "SWILL_Payload\MinHook\lib.x86" mkdir "SWILL_Payload\MinHook\lib.x86"

REM URL для загрузки MinHook
set MH_URL=https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.3.zip
set MH_ZIP=%TEMP%\minhook.zip
set MH_EXTRACT=%TEMP%\minhook_extract

echo [*] Скачивание MinHook v1.3.3...
powershell -Command "& {Invoke-WebRequest -Uri '%MH_URL%' -OutFile '%MH_ZIP%'}"

if not exist "%MH_ZIP%" (
    echo [!] Не удалось скачать MinHook. Попробуйте вручную.
    echo     URL: https://github.com/TsudaKageyu/minhook/releases
    goto MANUAL_SETUP
)

echo [*] Распаковка архива...
powershell -Command "& {Expand-Archive -Path '%MH_ZIP%' -DestinationPath '%MH_EXTRACT%' -Force}"

REM Копирование файлов MinHook
echo [*] Копирование файлов MinHook...

if exist "%MH_EXTRACT%\minhook-1.3.3\include\MinHook.h" (
    copy /Y "%MH_EXTRACT%\minhook-1.3.3\include\MinHook.h" "SWILL_Payload\MinHook\MinHook.h" > nul
    echo [+] MinHook.h скопирован
) else (
    echo [!] Файл MinHook.h не найден в архиве
)

if exist "%MH_EXTRACT%\minhook-1.3.3\bin\MinHook.x86.lib" (
    copy /Y "%MH_EXTRACT%\minhook-1.3.3\bin\MinHook.x86.lib" "SWILL_Payload\MinHook\lib.x86\MinHook.lib" > nul
    echo [+] MinHook.lib скопирован
) else if exist "%MH_EXTRACT%\minhook-1.3.3\bin\MinHook.lib" (
    copy /Y "%MH_EXTRACT%\minhook-1.3.3\bin\MinHook.lib" "SWILL_Payload\MinHook\lib.x86\MinHook.lib" > nul
    echo [+] MinHook.lib скопирован (альтернативное имя)
) else (
    echo [!] Файл MinHook.lib не найден. Будет использована встроенная реализация.
    echo     Проект сможет компилироваться, но хуки не будут работать корректно.
    echo     Рекомендуется скачать библиотеку вручную.
)

REM Очистка временных файлов
echo [*] Очистка временных файлов...
del /Q "%MH_ZIP%" > nul 2>&1
rmdir /S /Q "%MH_EXTRACT%" > nul 2>&1

echo.
echo ============================================================
echo                  Настройка завершена успешно!
echo ============================================================
echo.
echo Следующие шаги:
echo 1. Откройте SWILL_Project.sln в Visual Studio
echo 2. Выберите конфигурацию Release | Win32
echo 3. Соберите решение (Ctrl+Shift+B)
echo 4. Готовые файлы появятся в папке bin/
echo.
echo Для дополнительной информации см. README.md
echo ============================================================
pause
exit /b 0

:MANUAL_SETUP
echo.
echo ============================================================
echo               Ручная установка MinHook
echo ============================================================
echo.
echo 1. Перейдите на страницу: https://github.com/TsudaKageyu/minhook/releases
echo 2. Скачайте последнюю версию (например, minhook-v1.3.3.zip)
echo 3. Распакуйте архив
echo 4. Скопируйте файлы:
echo    - include\MinHook.h         -^> SWILL_Payload\MinHook\MinHook.h
echo    - bin\MinHook.x86.lib       -^> SWILL_Payload\MinHook\lib.x86\MinHook.lib
echo.
echo После копирования файлов запустите этот скрипт снова.
echo ============================================================
pause
exit /b 1
