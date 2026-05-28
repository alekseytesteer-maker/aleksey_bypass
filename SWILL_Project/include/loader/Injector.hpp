#pragma once

#include "shared/SharedDefs.hpp"
#include <string>
#include <vector>

namespace swill {

// Утилиты для работы с процессами
class ProcessUtils {
public:
    // Найти процесс по имени (возвращает первый найденный PID)
    static bool FindProcessByName(const std::wstring& processName, DWORD& outPid);
    
    // Найти все процессы с указанным именем
    static std::vector<DWORD> FindAllProcessesByName(const std::wstring& processName);
    
    // Получить базовый адрес модуля в процессе
    static bool GetModuleBaseAddress(DWORD pid, const std::wstring& moduleName, uintptr_t& baseAddr, SIZE_T& size);
    
    // Проверить, запущен ли процесс
    static bool IsProcessRunning(DWORD pid);
    
    // Ждать запуска процесса (с таймаутом)
    static bool WaitForProcess(const std::wstring& processName, int timeoutMs, DWORD& outPid);
};

// ============================================================================
// Injector - Внешний инжектор для загрузки SWILL Payload в процесс GTA
// Используется как альтернатива внутренней инъекции через loader.dll
// ============================================================================

class Injector {
public:
    // Внедрение DLL в указанный процесс по PID
    static bool InjectIntoProcess(DWORD processId, const char* dllPath);

    // Поиск процесса gta_sa.exe
    static DWORD FindGTAProcess();

    // Принудительный запуск GTA с инъекцией
    static bool LaunchAndInject(const char* gtaPath, const char* dllPath);

private:
    // Внутренняя функция инъекции через CreateRemoteThread + LoadLibrary
    static bool InjectViaLoadLibrary(HANDLE hProcess, const char* dllPath);

    // Альтернативная инъекция через SetThreadContext (для suspended процесса)
    static bool InjectViaSetThreadContext(HANDLE hProcess, const char* dllPath);
};

} // namespace swill
