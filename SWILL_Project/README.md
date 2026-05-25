# SWILL Project - Stable Windows Injection & Loading Library

**Версия:** 1.0.0  
**Платформа:** Windows x86 (Win32)  
**Цель:** Multi Theft Auto (GTA San Andreas)

## Описание

SWILL — это устойчивый инжектор DLL с сигнатурным сканированием и перехватом функций через MinHook. Проект предназначен для безопасного внедрения в процесс игры с обходом базовых защитных механизмов.

## Структура проекта

```
SWILL_Project/
├── 3rdparty/               # Внешние библиотеки
│   └── MinHook/            # Библиотека для перехвата функций
├── SWILL_Shared/           # Общие определения
│   └── SharedDefs.hpp      # Константы, сигнатуры, структуры
├── SWILL_Loader/           # Инжектор (EXE)
│   ├── Injector.hpp        # Логгер и RAII гарды
│   ├── Injector.cpp        # Реализация инжекта
│   └── Main.cpp            # Точка входа
├── SWILL_Payload/          # DLL с хуками
│   ├── Memory.hpp/cpp      # Сканер сигнатур и патчинг
│   ├── Hooks.hpp/cpp       # Обертка над MinHook
│   ├── Core.cpp            # Логика хуков (CIdArray)
│   └── dllmain.cpp         # DLL entry point
├── bin/                    # Выходные файлы сборки
└── docs/                   # Документация
```

## Ключевые функции

### Инжектор (SWILL_Loader)
- **SeDebugPrivilege** — получение прав отладки
- **RAII Handle Guards** — автоматическое управление дескрипторами
- **LoadLibrary Injection** — классический метод внедрения
- **Встроенный логгер** — потокобезопасное логирование

### Payload (SWILL_Payload)
- **AOB Pattern Scanner** — поиск функций по сигнатурам с проверкой страниц памяти
- **MinHook Integration** — безопасный перехват функций
- **__fastcall/__thiscall** — корректное соглашение для x86
- **SEH Protection** — защита от исключений доступа к памяти
- **Atomic Variables** — потокобезопасные глобальные состояния
- **Clean Shutdown** — восстановление оригинальных байтов при выгрузке

## Сборка

### Требования
- Visual Studio 2019 или новее
- Platform Toolset: v142 или новее
- Windows SDK 10.0+
- Библиотека MinHook (скачать отдельно)

### Шаги
1. Откройте `SWILL_Project.sln` в Visual Studio
2. Скачайте MinHook с https://github.com/TsudaKageyu/minhook
3. Поместите файлы MinHook в `3rdparty/MinHook/`
4. Выберите конфигурацию **Release | Win32**
5. Соберите решение (Ctrl+Shift+B)
6. Готовые файлы появятся в папке `bin/`

## Использование

1. Запустите GTA San Andreas с MTA
2. Запустите `SWILL_Loader.exe`
3. Инжектор автоматически найдет процесс и внедрит DLL
4. Проверьте логи:
   - `swill_injector.log` — лог инжектора
   - `swill_payload.log` — лог DLL в процессе игры

## Технические детали

### Перехват CIdArray::PopUniqueId
Функция использует соглашение `__thiscall` (x86), которое эмулируется через `__fastcall`:
- Первый аргумент (this) передается в ECX
- Второй аргумент (edx) — фиктивный для выравнивания
- Третий аргумент — параметр функции

### Защита от крашей
- Проверка указателей на nullptr перед вызовом
- SEH блоки (__try/__except) для чтения памяти
- Атомарные переменные для многопоточного доступа
- Виртуальные ID при пустом стеке

## Лицензия

Проект создан в образовательных целях. Используйте на свой страх и риск.

## Контакты

Документация: `docs/DOCUMENTATION.md`  
Инструкция по сборке: `BUILD_INSTRUCTIONS.md`
