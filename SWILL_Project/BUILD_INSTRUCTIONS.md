# Инструкция по сборке SWILL Project

## Предварительные требования

1. **Visual Studio 2019 или новее**
   - Установите компонент "Desktop development with C++"
   - Platform Toolset: v142 или новее

2. **Windows SDK 10.0+**

3. **Библиотека MinHook**
   - Скачайте с https://github.com/TsudaKageyu/minhook/releases
   - Распакуйте в папку `3rdparty/MinHook/`

## Настройка MinHook

### Вариант A: Использование готовых библиотек
1. Скачайте релиз MinHook
2. Скопируйте файлы:
   - `include/MinHook.h` → `3rdparty/MinHook/include/`
   - `lib/libMinHook.x86.lib` → `3rdparty/MinHook/lib.x86/`

### Вариант B: Компиляция из исходников
```bash
cd 3rdparty/minhook
cmake -B build -A Win32
cmake --build build --config Release
copy build\Release\libMinHook.lib ../lib.x86/
```

## Сборка проекта в Visual Studio

1. Откройте `SWILL_Project.sln`

2. Проверьте пути включения:
   - Правый клик на решении → Properties
   - Configuration Properties → C/C++ → General
   - Additional Include Directories: `$(ProjectDir)..\3rdparty\MinHook\include`

3. Проверьте пути библиотек:
   - Configuration Properties → Linker → General
   - Additional Library Directories: `$(ProjectDir)..\3rdparty\MinHook\lib.x86`

4. Выберите конфигурацию:
   - Solution Platforms: **Win32** (не x64!)
   - Configuration: **Release**

5. Соберите решение:
   - Build → Build Solution (Ctrl+Shift+B)

6. Результаты появятся в папке `bin/`:
   - `SWILL_Loader.exe` — инжектор
   - `SwillPayload.dll` — DLL с хуками

## Пост-сборка

### Внедрение DLL в EXE (опционально)

Для автоматического внедрения DLL в тело EXE:

1. Добавьте файл ресурсов `Resource.rc` в проект Loader:
```
SWILL_PAYLOAD RCDATA "SwillPayload.dll"
```

2. В Main.cpp добавьте:
```cpp
// Извлечение ресурса
HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(SWILL_PAYLOAD), RT_RCDATA);
HGLOBAL hData = LoadResource(NULL, hRes);
LPVOID pData = LockResource(hData);
DWORD size = SizeofResource(NULL, hRes);
```

## Отладка

### Логи
- `swill_injector.log` — лог инжектора (рабочая директория)
- `swill_payload.log` — лог DLL (директория игры)

### Отладочная сборка
1. Выберите конфигурацию **Debug**
2. Включите генерацию отладочной информации
3. Используйте Output Debug String для вывода в DbgView

## Решение проблем

### Ошибка линковки LNK1104: cannot open file 'MinHook.lib'
- Проверьте путь к библиотеке в настройках проекта
- Убедитесь что файл существует в `3rdparty/MinHook/lib.x86/`

### Ошибка компиляции: 'MH_Initialize' undefined
- Убедитесь что `MinHook.h` находится в правильном месте
- Проверьте Additional Include Directories

### DLL не загружается в процесс
- Запустите игру и инжектор от имени администратора
- Проверьте антивирус (может блокировать инъекцию)
- Используйте DbgView для просмотра ошибок

### Краш при выгрузке DLL
- Убедитесь что вызывается `SwillHooks::Shutdown()`
- Проверьте что все хуки сняты перед выходом

## Архитектурные заметки

### Почему Win32?
GTA San Andreas — 32-битное приложение. Инъекция 64-битной DLL невозможна.

### Почему __fastcall?
MSVC не поддерживает прямое объявление `__thiscall` функций. Используем `__fastcall` для эмуляции передачи this через ECX.

### Почему RAII?
Автоматическое управление ресурсами предотвращает утечки HANDLE при исключениях.
