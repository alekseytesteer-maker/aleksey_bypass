# SWILL Project - MTA:SA Anti-Cheat Bypass

## Описание
SWILL (Stealthy Wrapper for Intercepting Lua Logic) — проект для обхода античита MTA:SA версии 1.6-unstable-0.

## Компоненты

### 1. SWILL_Core (Static Library)
Базовая библиотека с общими функциями:
- **HookEngine** — кастомный движок inline-хуков (замена MinHook)
- **PatternScanner** — поиск байтовых сигнатур в памяти
- **MemoryUtils** — утилиты для работы с памятью
- **AntiTamper** — обход TamperGuard из netc.dll

### 2. SWILL_Payload (DLL)
Основной модуль для инъекции в процесс игры:
- **LuaIntegration** — интеграция с Lua VM MTA:SA
- **NetworkHooks** — перехват сетевых функций (блокировка AC репортов)
- **GuardBypass** — обход SEH Detour, Watchdog, WER очистка

### 3. SWILL_Loader (EXE)
Внешний инжектор для загрузки DLL:
- Инъекция в запущенный процесс gta_sa.exe
- Запуск GTA с автоматической инъекцией

## Сборка

### Требования
- Visual Studio 2015 или новее (для совместимости с MTA)
- CMake 3.15+
- Windows SDK

### Компиляция
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 14 2015"
cmake --build . --config Release
```

## Использование

### Режим 1: Инъекция в запущенный процесс
```bash
SWILL_Loader.exe --inject
```

### Режим 2: Запуск GTA с инъекцией
```bash
SWILL_Loader.exe --launch -p "C:\Path\To\gta_sa.exe"
```

### Режим 3: Ручная загрузка
Загрузите `SWILL.dll` через любой инжектор (Manual Map, LoadLibrary, и т.д.)

## Обход защиты

### TamperGuard (netc.dll)
- Отключение флага `g_tamper_lock`
- Патч INT 0x29 (__fastfail) инструкций
- Обход XOR guard-функций

### Loader.dll проверки
- Очистка WER дампов перед запуском
- Сброс реестровых флагов (uncleanstop, lastruncrash)
- Переменная среды `MTA_DISABLE_SEH_DETOUR`

### Сетевые хуки
- Блокировка пакета ID 91 (TRANSGRESSION)
- Фильтрация AC_Logger вызовов (ID: 9734, 9736, 8250, 8648, 7060, 7744, 7745)
- Эмуляция успешных AC_Pulsator проверок

## Структура проекта
```
SWILL_Project/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── shared/
│   │   └── SharedDefs.hpp
│   ├── core/
│   │   ├── HookEngine.hpp
│   │   ├── PatternScanner.hpp
│   │   ├── MemoryUtils.hpp
│   │   └── AntiTamper.hpp
│   ├── payload/
│   │   ├── LuaIntegration.hpp
│   │   ├── NetworkHooks.hpp
│   │   └── GuardBypass.hpp
│   └── loader/
│       └── Injector.hpp
└── src/
    ├── core/
    │   └── HookEngine.cpp
    ├── payload/
    │   ├── dllmain.cpp
    │   ├── LuaIntegration.cpp
    │   ├── NetworkHooks.cpp
    │   └── GuardBypass.cpp
    └── loader/
        ├── main.cpp
        └── Injector.cpp
```

## Предупреждение
Использование данного ПО может нарушать условия обслуживания MTA:SA и серверов. Используйте на свой страх и риск только в образовательных целях.

## Лицензия
MIT License
