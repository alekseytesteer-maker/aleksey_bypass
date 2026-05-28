#pragma once

#include "shared/SharedDefs.hpp"

namespace swill {

// ============================================================================
// GuardBypass - Обход дополнительных защитных механизмов MTA
// Включает: SEH Detour, Watchdog, Loader checks
// ============================================================================

class GuardBypass {
public:
    // Инициализация всех обходов
    static bool Initialize();
    
    // Отключение SEH Detour Protection (core.dll)
    static bool DisableSEHDetour();
    
    // Сброс флагов Watchdog (uncleanstop, lastruncrash)
    static bool ResetWatchdogFlags();
    
    // Очистка WER дампов перед запуском
    static bool CleanWERDumps();
    
    // Подмена кода выхода процесса (для обхода AC integrity exit)
    static bool SpoofExitCode(int fakeExitCode = 0);
    
    // Скрытие процессов из blacklist (CheatEngine, PCHunter)
    static bool HideBlacklistedProcesses();

private:
    // Вспомогательные функции
    static bool SetEnvironmentVariableSafe(const char* name, const char* value);
    static bool DeleteRegistryValue(const char* keyPath, const char* valueName);
};

} // namespace swill
