# SWILL Project — Visual Studio 2015 Solution Configuration

## 🚀 Автоматизированная сборка через Visual Studio

### Шаг 1: Открытие решения

1. Запустите **Visual Studio 2015** (или новее)
2. Файл → Открыть → Проект/Решение
3. Выберите файл: `SWILL_Project/build/SWILL.sln`

> ⚠️ Если файл `.sln` ещё не создан, сначала выполните команду:
> ```cmd
> cd SWILL_Project
> mkdir build && cd build
> cmake .. -G "Visual Studio 14 2015" -A Win32
> ```

---

### Шаг 2: Настройка конфигурации

В верхней панели инструментов установите:

| Параметр | Значение |
|----------|----------|
| Конфигурация | **Release** |
| Платформа | **Win32** (не x64!) |

![Config](https://i.imgur.com/example.png)

---

### Шаг 3: Сборка решения

**Вариант A: Через меню**
- Сборка → Сборка решения (Ctrl+Shift+B)

**Вариант B: Через панель Toolbox**
- Нажмите кнопку "Build" на панели инструментов

**Вариант C: Горячие клавиши**
- `Ctrl+Shift+B` — собрать всё решение
- `F7` — собрать выбранный проект

---

### Шаг 4: Проверка результата

После успешной сборки в окне **Output** (Вывод) вы увидите:

```
========== Build: 3 succeeded, 0 failed ==========
```

Бинарники будут расположены в:
```
SWILL_Project/build/bin/Release/
  ├── SWILL_Core.lib
  ├── SWILL_Payload.dll
  ├── SWILL_Payload.pdb
  └── SWILL_Loader.exe
```

---

## 🔧 Расширенные настройки Visual Studio

### Включение debug-символов

Для отладки с символами:

1. Правой кнопкой на проекте → Свойства
2. Конфигурация: **All Configurations**
3. C/C++ → General → Debug Information Format: **Program Database (/Zi)**
4. Linker → Debugging → Generate Debug Info: **Yes (/DEBUG)**

### Оптимизация для Release

Проверьте настройки оптимизации:

1. Проект → Свойства
2. C/C++ → Optimization:
   - Optimization: **Maximize Speed (/O2)**
   - Inline Function Expansion: **Any Suitable (/Ob2)**
   - Enable FPO: **Yes (/Oy)**

### Статическая компоновка CRT (опционально)

Чтобы избежать зависимости от VC++ Redist:

1. Проект → Свойства
2. C/C++ → Code Generation
3. Runtime Library: **Multi-threaded (/MT)** (для Release)
4. Runtime Library: **Multi-threaded Debug (/MTd)** (для Debug)

> ⚠️ Внимание: статическая компоновка увеличивает размер DLL

---

## 🐛 Отладка в Visual Studio

### Запуск с отладчиком

1. Откройте свойства проекта **SWILL_Loader**
2. Configuration Properties → Debugging
3. Command Arguments: `--pid <PID_игры>`
4. Нажмите F5 для запуска с отладкой

### Attach к процессу игры

Для отладки уже запущенной игры:

1. Отладка → Присоединить к процессу (Ctrl+Alt+P)
2. Найдите `gta_sa.exe` в списке
3. Нажмите "Attach"
4. Установите breakpoints в коде SWILL
5. Выполните действие в игре для триггера хука

### Просмотр логов

Отладочный вывод можно увидеть в:
- **Output** окно (отладка Visual Studio)
- Файл `swill_log.txt` в папке с игрой
- **DebugView** (Sysinternals) для перехвата OutputDebugString

---

## 📦 Экспорт настроек для других разработчиков

Чтобы поделиться настройками проекта:

1. Файл → Export Templates
2. Выберите проект или решение
3. Сохраните как `.vssettings` файл

Или просто закоммитьте в Git:
```
build/SWILL.sln
build/SWILL.v14.suo (игнорировать в .gitignore)
CMakeLists.txt
```

---

## ⚡ Быстрые команды через Developer Command Prompt

Если предпочитаете командную строку:

```cmd
:: Открыть Developer Command Prompt for VS2015

:: Перейти в директорию сборки
cd C:\Path\To\SWILL_Project\build

:: Сборка через MSBuild
msbuild SWILL.sln /p:Configuration=Release /p:Platform=Win32

:: Сборка с пересборкой всех файлов
msbuild SWILL.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32

:: Сборка только Payload DLL
msbuild SWILL_Payload.vcxproj /p:Configuration=Release /p:Platform=Win32
```

---

## 🎯 Интеграция с CI/CD (AppVeyor / GitHub Actions)

Пример для **GitHub Actions** (`.github/workflows/build.yml`):

```yaml
name: Build SWILL

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-2019
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Configure CMake
      run: |
        mkdir build
        cd build
        cmake .. -G "Visual Studio 14 2015" -A Win32
    
    - name: Build
      run: |
        cd build
        cmake --build . --config Release
    
    - name: Upload Artifacts
      uses: actions/upload-artifact@v2
      with:
        name: SWILL-Binaries
        path: build/bin/Release/
```

---

## 📞 Диагностика проблем Visual Studio

### Проблема: "Platform toolset v140 not found"

**Решение:**
1. Панель управления → Программы и компоненты
2. Найдите Visual Studio 2015
3. Изменить → Убедитесь что установлен "Visual C++"
4. Или установите более новую версию и выберите совместимый toolset

### Проблема: Ошибка линковки LNK1104 (не может открыть lib)

**Решение:**
1. Проверьте что все проекты в решении собраны
2. Очистите решение (Сборка → Очистить решение)
3. Пересоберите (Сборка → Перестроить решение)

### Проблема: DLL не загружается в игру при отладке

**Решение:**
1. Убедитесь что собран **Win32**, не x64
2. Проверьте пути к зависимостям (Lua, etc.)
3. Запустите Visual Studio от имени администратора
4. Отключите антивирус на время отладки

---

**Версия документации:** 1.0  
**Совместимость:** Visual Studio 2015 Update 3+  
**Дата обновления:** 2026-05-28
