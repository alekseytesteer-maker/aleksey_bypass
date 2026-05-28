#pragma once

#include "shared/SharedDefs.hpp"

namespace swill {

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
