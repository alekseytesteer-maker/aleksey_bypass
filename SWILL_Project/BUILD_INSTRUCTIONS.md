# Инструкция по быстрой сборке SWILL Project

## Вариант 1: Использование Visual Studio (Рекомендуется)

### Шаг 1: Подготовка MinHook
1. Скачайте оригинальную библиотеку MinHook: https://github.com/TsudaKageyu/minhook/releases
2. Распакуйте архив
3. Скопируйте `MinHook.h` из `minhook-devel/include/` в `SWILL_Payload/MinHook/`
4. Скопируйте `MinHook.lib` из `minhook-devel/lib/MinHook.x86.lib` в `SWILL_Payload/MinHook/lib.x86/MinHook.lib`

### Шаг 2: Открытие проекта
1. Откройте `SWILL_Project.sln` в Visual Studio 2019 или 2022
2. Убедитесь что выбрана конфигурация **Release** и платформа **x86**

### Шаг 3: Сборка
1. Нажмите `Ctrl+Shift+B` или выберите `Build -> Build Solution`
2. Дождитесь успешной сборки обоих проектов

### Шаг 4: Запуск
1. В папке `bin/` появятся два файла:
   - `SWILL_Loader.exe` - инжектор
   - `SwillPayload.dll` - DLL ядро
2. **ВАЖНО**: Скопируйте `SwillPayload.dll` в ту же папку, где находится `SWILL_Loader.exe`
3. Запустите GTA SA с MTA
4. Запустите `SWILL_Loader.exe` от имени администратора
5. Инжектор автоматически найдет процесс и внедрит DLL

---

## Вариант 2: Сборка через командную строку (MSBuild)

```batch
# Откройте Developer Command Prompt for VS
cd SWILL_Project

# Сборка решения
msbuild SWILL_Project.sln /p:Configuration=Release /p:Platform=Win32
```

---

## Проверка работы

После запуска вы должны увидеть:
1. Консоль инжектора с сообщением об успехе
2. В игре появится новая консоль "SWILL Core v1.0 - Debug Console"
3. В папке с игрой создастся файл `swill_payload.log`

---

## Возможные проблемы и решения

### Проблема: Ошибка компиляции MinHook
**Решение**: Убедитесь что скачали оригинальную библиотеку MinHook и правильно разместили файлы

### Проблема: DLL не загружается
**Решение**: 
- Проверьте что DLL и EXE в одной папке
- Запустите инжектор от имени администратора
- Отключите антивирус на время тестирования

### Проблема: netc.dll не найден
**Решение**: Убедитесь что запустили GTA SA с MTA до запуска инжектора

### Проблема: Вылет игры с кодом 0xC0000409
**Решение**: 
- Проверьте что используете x86 версию DLL (не x64)
- Убедитесь что сигнатуры актуальны для вашей версии игры

---

## Структура файлов после сборки

```
SWILL_Project/
├── bin/
│   ├── SWILL_Loader.exe      # Инжектор
│   └── SwillPayload.dll      # DLL ядро (скопировать рядом с Loader)
├── obj/                       # Промежуточные файлы сборки
├── SWILL_Loader/
│   ├── Injector.hpp
│   └── Main.cpp
├── SWILL_Payload/
│   ├── MinHook/
│   │   ├── MinHook.h
│   │   ├── MinHook.cpp
│   │   └── lib.x86/
│   │       └── MinHook.lib
│   ├── Memory.hpp
│   ├── Hooks.hpp
│   ├── Logger.hpp
│   ├── Core.cpp
│   └── dllmain.cpp
├── SWILL_Project.sln
├── SWILL_Loader.vcxproj
├── SWILL_Payload.vcxproj
└── README.md
```

---

## Технические характеристики

- **Архитектура**: x86 (32-bit)
- **Стандарт C++**: C++17
- **Инструменты**: Visual Studio 2019/2022 (v142 toolset)
- **Библиотеки**: MinHook, Psapi
- **Совместимость**: Windows 7/8/10/11
