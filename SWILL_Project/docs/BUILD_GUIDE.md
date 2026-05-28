# SWILL Project — Полная документация по сборке и запуску

## 📋 Описание проекта

**SWILL** (Stealth Workload Injection & Lua Layer) — фреймворк для безопасной инъекции кода в MTA:SA 1.6-unstable-0 с обходом системы защиты TamperGuard и интеграцией с Lua VM.

### Ключевые возможности
- ✅ Собственный движок inline-хуков (без сторонних библиотек)
- ✅ Обход TamperGuard (netc.dll) с патчем INT 0x29
- ✅ Блокировка античит-пакетов (ID: 91, 9736, 8250, 8648, 7060, 7744, 7745)
- ✅ Очистка WER дампов и сброс флагов watchdog
- ✅ Интеграция с Lua VM для выполнения скриптов в рантайме
- ✅ Автоматический поиск процесса GTA и инъекция

---

## 🏗️ Структура проекта

```
SWILL_Project/
├── CMakeLists.txt              # Главный файл сборки
├── README.md                   # Этот файл
├── docs/
│   └── BUILD_GUIDE.md          # Подробная инструкция
├── src/
│   ├── Core/
│   │   ├── HookEngine.hpp      # Движок хуков
│   │   ├── HookEngine.cpp
│   │   ├── PatternScanner.hpp  # Поиск байт-паттернов
│   │   ├── PatternScanner.cpp
│   │   ├── MemoryUtils.hpp     # Утилиты памяти
│   │   ├── MemoryUtils.cpp
│   │   └── AntiTamper.hpp      # Обход TamperGuard
│   │   └── AntiTamper.cpp
│   ├── Payload/
│   │   ├── main.cpp            # Точка входа DLL
│   │   ├── NetworkHooks.hpp    # Сетевые хуки
│   │   ├── NetworkHooks.cpp
│   │   ├── GuardBypass.hpp     # Обход защит
│   │   ├── GuardBypass.cpp
│   │   └── LuaIntegration.hpp  # Интеграция с Lua
│   │   └── LuaIntegration.cpp
│   └── Loader/
│       ├── main.cpp            # Точка входа EXE
│       ├── Injector.hpp        # Логика инъекции
│       ├── Injector.cpp
│       ├── ProcessUtils.hpp    # Утилиты процессов
│       └── ProcessUtils.cpp
└── build/                      # Директория сборки (создаётся автоматически)
```

---

## 🛠️ Требования для сборки

### Обязательные компоненты
1. **Visual Studio 2015 Update 3** (или новее)
   - workload: "Разработка классических приложений на C++"
   - Platform Toolset: `v140`

2. **CMake 3.10+**
   - Скачать: https://cmake.org/download/
   - Добавить в PATH

3. **Windows SDK 10.0.10586.0+**
   - Обычно устанавливается с Visual Studio

### Опционально (для отладки)
- x32dbg / OllyDbg — для анализа паттернов
- Process Hacker — для мониторинга процессов
- Lua 5.1 — для тестирования скриптов

---

## 🚀 Быстрый старт (Automated Build)

### Вариант 1: PowerShell скрипт (рекомендуется)

```powershell
# Запустить из корня проекта
cd C:\Path\To\SWILL_Project

# Создать директорию сборки
mkdir build
cd build

# Конфигурация для Visual Studio 2015 (Win32)
cmake .. -G "Visual Studio 14 2015" -A Win32

# Сборка Release версии
cmake --build . --config Release

# Результат в: build/bin/Release/
```

### Вариант 2: Visual Studio IDE

1. Откройте `build/SWILL.sln` в Visual Studio
2. Выберите конфигурацию: `Release` | `Win32`
3. Сборка → Сборка решения (Ctrl+Shift+B)
4. Бинарники появятся в `build/bin/Release/`

### Вариант 3: Командная строка Developer Command Prompt

```cmd
:: Открыть "Developer Command Prompt for VS2015"
cd C:\Path\To\SWILL_Project

mkdir build && cd build
cmake .. -G "Visual Studio 14 2015" -A Win32
msbuild SWILL.sln /p:Configuration=Release /p:Platform=Win32
```

---

## 📦 Результат сборки

После успешной сборки в директории `build/bin/Release/` появятся:

| Файл | Назначение |
|------|-----------|
| `SWILL_Core.lib` | Статическая библиотека (ядерные функции) |
| `SWILL_Payload.dll` | Основная DLL для инъекции в GTA |
| `SWILL_Loader.exe` | Внешний инжектор для запуска DLL |
| `SWILL_Payload.pdb` | Отладочные символы (для дизассемблирования) |

---

## 🎯 Использование

### Метод 1: Через SWILL_Loader (автоматическая инъекция)

```cmd
:: Запуск инжектора (найти процесс и внедрить DLL)
SWILL_Loader.exe

:: Или указать PID вручную
SWILL_Loader.exe --pid 12345
```

**Алгоритм работы Loader:**
1. Сканирует процессы на наличие `gta_sa.exe`
2. Открывает процесс с правами `PROCESS_ALL_ACCESS`
3. Выделяет память в целевом процессе
4. Загружает `SWILL_Payload.dll` через `LoadLibraryA`
5. Освобождает ресурсы и завершает работу

### Метод 2: Ручная инъекция (через x32dbg/Injectors)

1. Загрузите `SWILL_Payload.dll` в процесс `gta_sa.exe` любым инжектором
2. DLL автоматически выполнит `DllMain` → `InitializeSWILL()`
3. Проверьте лог `swill_log.txt` в папке с игрой

### Метод 3: Интеграция в существующий чит

```cpp
// Подключить заголовки
#include "HookEngine.hpp"
#include "NetworkHooks.hpp"
#include "LuaIntegration.hpp"

// Инициализировать в DllMain вашей DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, InitializeSWILL, hModule, 0, nullptr);
    }
    return TRUE;
}
```

---

## ⚙️ Конфигурация

### Переменные среды (опционально)

```cmd
:: Отключить SEH Detour Protection в core.dll
set MTA_DISABLE_SEH_DETOUR=1

:: Включить debug-логирование
set SWILL_DEBUG=1

:: Путь к лог-файлу
set SWILL_LOG_PATH=C:\MTA\swill_debug.log
```

### Паттерны для поиска функций (auto-detect)

Проект автоматически сканирует модули `netc.dll`, `core.dll`, `client.dll` для поиска адресов функций. Если сигнатуры устарели:

1. Откройте `src/Core/PatternScanner.cpp`
2. Обновите байт-паттерны в массиве `patterns[]`
3. Пересоберите проект

**Пример обновления паттерна:**
```cpp
// Было (версия 1.5.9)
{ "SendPacket", "netc.dll", "\x55\x8B\xEC\x6A\xFF...", "xxxxxx..." }

// Стало (версия 1.6-unstable-0)
{ "SendPacket", "netc.dll", "\x55\x8B\xEC\x53\x56...", "xxxxxx..." }
```

---

## 🐛 Отладка и логирование

### Файлы логов

| Файл | Описание |
|------|----------|
| `swill_log.txt` | Основной лог работы SWILL |
| `mta\core.log` | Лог ядра MTA (ошибки защиты) |
| `mta\logs\console.log` | Консоль MTA |

### Включение debug-режима

В файле `src/Payload/main.cpp` измените:
```cpp
#define SWILL_DEBUG 1  // Было: 0
```

### Проверка обхода защиты

1. Запустите игру с внедрённой DLL
2. Попробуйте выполнить запрещённое действие (например, изменить память vtable)
3. Если игра не вылетела с `INT 0x29` — обход работает
4. Проверьте `swill_log.txt` на наличие `[AntiTamper] g_tamper_lock disabled`

---

## 🔧 Расширение функционала

### Добавление нового хука

```cpp
// 1. Объявить функцию-хук в NetworkHooks.hpp
typedef bool (__thiscall* SendPacket_t)(void*, uchar, void*, int, int, int);
extern SendPacket_t Original_SendPacket;
bool __fastcall Hooked_SendPacket(void* ECX, void* EDX, uchar id, void* bs, int p1, int p2, int p3);

// 2. Реализовать в NetworkHooks.cpp
SendPacket_t Original_SendPacket = nullptr;

bool __fastcall Hooked_SendPacket(...)
{
    if (id == 99) // Новый пакет для блокировки
        return true; // Блокировать
    
    return Original_SendPacket(ECX, id, bs, p1, p2, p3);
}

// 3. Зарегистрировать в InitializeSWILL()
Original_SendPacket = (SendPacket_t)HookEngine::CreateHook(
    PatternScanner::FindPattern("netc.dll", "SendPacket"),
    (void*)Hooked_SendPacket
);
```

### Добавление Lua-функции

```cpp
// В LuaIntegration.cpp
int lua_my_custom_function(lua_State* L)
{
    const char* arg = luaL_checkstring(L, 1);
    outputDebugString("[SWILL] Called with: " + std::string(arg));
    return 0;
}

// Регистрация в InitializeLua()
lua_register(L, "myCustomFunction", lua_my_custom_function);
```

---

## ⚠️ Известные ограничения

1. **Только Windows x86** — проект компилируется только под 32-битную архитектуру
2. **Visual Studio 2015+** — требуется совместимость с ABI v140
3. **MTA:SA 1.6-unstable-0** — паттерны актуальны для этой версии (может потребоваться обновление для других)
4. **Admin rights** — для инъекции могут потребоваться права администратора

---

## 📞 Поддержка и диагностика проблем

### Проблема: Сборка не начинается

**Решение:**
```cmd
:: Очистить кэш CMake
rmdir /s /q build
mkdir build
cd build

:: Запустить с verbose-логами
cmake .. -G "Visual Studio 14 2015" -A Win32 --debug-output
```

### Проблема: DLL не загружается в игру

**Диагностика:**
1. Проверьте архитектуру (должна быть x86, не x64)
2. Убедитесь что все зависимости установлены (VC++ 2015 Redist)
3. Запустите от имени администратора
4. Проверьте антивирус (может блокировать инъекции)

### Проблема: Игра вылетает с крашем

**Решение:**
1. Проверьте `mta\core.log` на наличие `STATUS_FAIL_FAST_EXCEPTION`
2. Убедитесь что `g_tamper_lock` отключён (смотрите `swill_log.txt`)
3. Обновите паттерны в `PatternScanner.cpp`

---

## 📄 Лицензия

Проект создан исключительно в образовательных целях для изучения архитектуры MTA:SA и механизмов защиты. Все права на оригинальный код MTA принадлежат разработчикам Multi Theft Auto.

---

## 📚 Дополнительные ресурсы

- [Документация по архитектуре MTA](docs/ARCHITECTURE.md)
- [Справочник Lua API client.dll](docs/LUA_API_REFERENCE.md)
- [Анализ TamperGuard](docs/TAMPERGUARD_ANALYSIS.md)

---

**Дата обновления документации:** 2026-05-28  
**Версия проекта:** 1.0.0  
**Совместимость:** MTA:SA 1.6-unstable-0 (build kC5Ieu6Wu)
