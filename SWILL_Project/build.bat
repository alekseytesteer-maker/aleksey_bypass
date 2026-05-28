@echo off
:: SWILL Project — Автоматизированный скрипт сборки для Windows
:: Требования: Visual Studio 2015+, CMake 3.10+

setlocal enabledelayedexpansion

echo ============================================
echo   SWILL Project - Automated Build Script
echo ============================================
echo.

:: Проверка наличия CMake
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake не найден в PATH!
    echo Скачайте и установите: https://cmake.org/download/
    pause
    exit /b 1
)

:: Проверка наличия Visual Studio
set VS_PATH=
if exist "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" (
    set VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat
) else if exist "C:\Program Files\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" (
    set VS_PATH=C:\Program Files\Microsoft Visual Studio 14.0\VC\vcvarsall.bat
)

if "%VS_PATH%"=="" (
    echo [WARNING] Visual Studio 2015 не найден в стандартных путях.
    echo Убедитесь что установлен Visual Studio 2015 с компонентом C++.
    echo.
)

:: Переход в директорию проекта
cd /d "%~dp0"

:: Очистка предыдущей сборки
if exist "build" (
    echo [INFO] Очистка предыдущей сборки...
    rmdir /s /q build
)

:: Создание директории сборки
echo [INFO] Создание директории сборки...
mkdir build
cd build

:: Конфигурация CMake
echo [INFO] Конфигурация проекта (Visual Studio 2015, Win32)...
cmake .. -G "Visual Studio 14 2015" -A Win32

if %errorlevel% neq 0 (
    echo [ERROR] Ошибка конфигурации CMake!
    pause
    exit /b 1
)

:: Сборка Release версии
echo [INFO] Сборка Release версии...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo [ERROR] Ошибка сборки!
    pause
    exit /b 1
)

:: Проверка результата
echo.
echo ============================================
echo   Сборка завершена успешно!
echo ============================================
echo.
echo Бинарники расположены в:
echo   %CD%\bin\Release\
echo.
echo Файлы:
dir /b bin\Release\*.dll bin\Release\*.exe bin\Release\*.lib 2>nul
echo.

:: Копирование в удобную директорию
if not exist "..\output" mkdir ..\output
copy bin\Release\SWILL_Payload.dll ..\output\ >nul
copy bin\Release\SWILL_Loader.exe ..\output\ >nul
copy bin\Release\SWILL_Core.lib ..\output\ >nul

echo [INFO] Копии файлов также доступны в: %CD%\..\output\
echo.

pause
