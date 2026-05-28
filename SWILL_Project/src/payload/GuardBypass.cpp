#include "payload/GuardBypass.hpp"
#include "core/MemoryUtils.hpp"
#include <shlobj.h>
#include <comdef.h>

#pragma comment(lib, "ole32.lib")

namespace swill {

bool GuardBypass::Initialize() {
    bool success = true;
    
    // 1. Отключение SEH Detour через переменную среды
    success &= DisableSEHDetour();
    
    // 2. Сброс флагов Watchdog
    success &= ResetWatchdogFlags();
    
    // 3. Очистка WER дампов
    success &= CleanWERDumps();
    
    return success;
}

bool GuardBypass::DisableSEHDetour() {
    // Установка переменной среды для отключения SEH Detour Protection
    // MTA проверяет: MTA_DISABLE_SEH_DETOUR
    return SetEnvironmentVariableSafe("MTA_DISABLE_SEH_DETOUR", "1");
}

bool GuardBypass::ResetWatchdogFlags() {
    // Сброс реестровых флагов watchdog
    // Ключ: Software\Multi Theft Auto: Province All\MTA San Andreas All\watchdog
    
    bool success = true;
    
    success &= DeleteRegistryValue(
        "Software\\Multi Theft Auto: Province All\\MTA San Andreas All\\watchdog",
        "uncleanstop");
    
    success &= DeleteRegistryValue(
        "Software\\Multi Theft Auto: Province All\\MTA San Andreas All\\watchdog",
        "lastruncrash");
    
    success &= DeleteRegistryValue(
        "Software\\Multi Theft Auto: Province All\\MTA San Andreas All",
        "debugger-crash-capture");
    
    return success;
}

bool GuardBypass::CleanWERDumps() {
    // Очистка Windows Error Reporting дампов для gta_sa.exe
    // Путь: %ProgramData%\Microsoft\Windows\WER\ReportArchive\AppCrash_gta_sa.exe_*
    
    wchar_t programData[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, programData))) {
        return false;
    }
    
    std::wstring werPath = std::wstring(programData) + 
                           L"\\Microsoft\\Windows\\WER\\ReportArchive";
    
    // Поиск и удаление файлов AppCrash_gta_sa.exe_*
    WIN32_FIND_DATAW findData;
    std::wstring searchPattern = werPath + L"\\AppCrash_gta_sa.exe_*";
    
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return true; // Нет файлов для удаления
    }
    
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring fullPath = werPath + L"\\" + findData.cFileName;
            DeleteFileW(fullPath.c_str());
        }
    } while (FindNextFileW(hFind, &findData) != 0);
    
    FindClose(hFind);
    
    // Также удаляем failfast дампы
    searchPattern = werPath + L"\\failfast_*.dmp";
    hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::wstring fullPath = werPath + L"\\" + findData.cFileName;
                DeleteFileW(fullPath.c_str());
            }
        } while (FindNextFileW(hFind, &findData) != 0);
        FindClose(hFind);
    }
    
    return true;
}

bool GuardBypass::SpoofExitCode(int fakeExitCode) {
    // Эта функция должна вызываться при выходе из процесса
    // Для подмены кода выхода можно использовать патч в exit-обработчике
    
    // Временная реализация: просто устанавливаем код выхода
    // Для полноценной подмены нужен хук на ExitProcess/NtTerminateProcess
    ExitProcess(fakeExitCode);
    return true;
}

bool GuardBypass::HideBlacklistedProcesses() {
    // Скрытие процессов из blacklist loader.dll
    // Черный список: CheatEngine, PCHunter
    
    // Примечание: полноценное скрытие требует драйвера или сложного хука
    // NtQuerySystemInformation. Здесь представлена базовая заглушка.
    
    // Можно реализовать через хук NtQuerySystemInformation
    // который фильтрует результаты Process32First/Next
    
    return true; // Заглушка
}

bool GuardBypass::SetEnvironmentVariableSafe(const char* name, const char* value) {
    return SetEnvironmentVariableA(name, value) != FALSE;
}

bool GuardBypass::DeleteRegistryValue(const char* keyPath, const char* valueName) {
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, keyPath, 0, KEY_SET_VALUE, &hKey);
    
    if (result == ERROR_SUCCESS) {
        RegDeleteValueA(hKey, valueName);
        RegCloseKey(hKey);
        return true;
    }
    
    // Пробуем HKLM если не нашли в HKCU
    result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_SET_VALUE, &hKey);
    if (result == ERROR_SUCCESS) {
        RegDeleteValueA(hKey, valueName);
        RegCloseKey(hKey);
        return true;
    }
    
    return false;
}

} // namespace swill
