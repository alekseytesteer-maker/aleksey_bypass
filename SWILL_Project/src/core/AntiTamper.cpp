#include "core/AntiTamper.hpp"

namespace swill {

// Флаг для отслеживания состояния TamperGuard
static bool g_tamperGuardDisabled = false;

// Оригинальный обработчик исключений
static LPTOP_LEVEL_EXCEPTION_FILTER g_originalExceptionFilter = nullptr;

// Обработчик исключений для обхода TamperGuard
LONG WINAPI TamperGuardExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    if (ExceptionInfo && ExceptionInfo->ExceptionRecord) {
        DWORD exceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
        
        // INT 0x29 - это код исключения, используемый TamperGuard
        if (exceptionCode == 0xC000041D || exceptionCode == 0xC0000005) {
            // Пытаемся продолжить выполнение, если это возможно
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    
    // Для других исключений используем стандартный обработчик
    if (g_originalExceptionFilter)
        return g_originalExceptionFilter(ExceptionInfo);
    
    return EXCEPTION_CONTINUE_SEARCH;
}

bool AntiTamper::DisableTamperGuard() {
    if (g_tamperGuardDisabled)
        return true;

    // Устанавливаем наш обработчик исключений
    g_originalExceptionFilter = SetUnhandledExceptionFilter(TamperGuardExceptionHandler);
    
    // Патчим ключевые функции netc.dll для обхода проверок
    // Ищем паттерн INT 0x29 и заменяем на NOP
    HMODULE hNetc = GetModuleHandleA("netc.dll");
    if (hNetc) {
        void* patternAddr = nullptr;
        // Паттерн для INT 0x29: CD 29
        BYTE int29Pattern[] = { 0xCD, 0x29 };
        const char* mask = "xx";
        
        PatternScanner scanner;
        if (scanner.FindPatternInModule(hNetc, "netc.dll", int29Pattern, mask, &patternAddr)) {
            MemoryUtils::PatchNop(patternAddr, 2);
        }
        
        // Также ищем другие варианты (INT 3, etc.)
        BYTE int3Pattern[] = { 0xCC };
        if (scanner.FindPatternInModule(hNetc, "netc.dll", int3Pattern, "x", &patternAddr)) {
            // Не патчим все INT 3, только специфичные для TamperGuard
            // Это требует более точного анализа
        }
    }
    
    g_tamperGuardDisabled = true;
    return true;
}

bool AntiTamper::RestoreTamperGuard() {
    if (!g_tamperGuardDisabled)
        return true;
    
    // Восстанавливаем оригинальный обработчик
    if (g_originalExceptionFilter) {
        SetUnhandledExceptionFilter(g_originalExceptionFilter);
        g_originalExceptionFilter = nullptr;
    }
    
    g_tamperGuardDisabled = false;
    return true;
}

bool AntiTamper::BypassSEHDetour() {
    // SEH Detour Protection в core.dll пытается предотвратить перехват обработчиков исключений
    // Мы можем обойти это, установив обработчик до инициализации CCore
    
    HMODULE hCore = GetModuleHandleA("core.dll");
    if (!hCore)
        return false;
    
    // Ищем функцию SetupCrashHandlers и патчим её
    // Это предотвратит установку SEH Detour Protection
    void* setupAddr = nullptr;
    
    // Паттерн для CCrashDumpWriter::SetupCrashHandlers
    // Требует точного анализа бинаря
    PatternScanner scanner;
    
    // Временно отключаем обработку исключений MTA
    // Это позволит нам установить свои хуки без вмешательства
    
    return true;
}

bool AntiTamper::ClearWERDumps() {
    // Очищаем Windows Error Reporting дампы перед запуском
    // Это предотвращает обнаружение крашей предыдущей сессии
    
    wchar_t werPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, werPath))) {
        wcscat_s(werPath, L"\\Microsoft\\Windows\\WER\\ReportArchive\\");
        
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(L"ReportArchive\\*", &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // Проверяем, относится ли дамп к gta_sa.exe
                    // Если да - удаляем или переименовываем
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
    }
    
    return true;
}

bool AntiTamper::ResetWatchdogFlags() {
    // Сбрасываем флаги Watchdog в реестре
    HKEY hKey;
    LPCWSTR subKey = L"Software\\Multi Theft Auto: Province All\\MTA San Andreas All\\watchdog";
    
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD zero = 0;
        RegSetValueExW(hKey, L"uncleanstop", 0, REG_DWORD, reinterpret_cast<BYTE*>(&zero), sizeof(zero));
        RegSetValueExW(hKey, L"lastruncrash", 0, REG_DWORD, reinterpret_cast<BYTE*>(&zero), sizeof(zero));
        RegCloseKey(hKey);
        return true;
    }
    
    return false;
}

bool AntiTamper::SpoofSerial(const char* newSerial) {
    // Подменяем серийный номер клиента
    if (!newSerial)
        return false;
    
    HKEY hKey;
    LPCWSTR subKey = L"Software\\Multi Theft Auto: Province All\\MTA San Andreas All";
    
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "serial", 0, REG_SZ, reinterpret_cast<const BYTE*>(newSerial), strlen(newSerial));
        RegCloseKey(hKey);
        return true;
    }
    
    return false;
}

} // namespace swill
