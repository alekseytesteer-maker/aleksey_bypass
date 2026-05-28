// SWILL Payload - DLL для инъекции в процесс MTA:SA
// Основной модуль, загружаемый в адресное пространство игры

#include "shared/SharedDefs.hpp"
#include "core/HookEngine.hpp"
#include "core/AntiTamper.hpp"
#include "payload/LuaIntegration.hpp"
#include "payload/NetworkHooks.hpp"
#include "payload/GuardBypass.hpp"

#include <Windows.h>
#include <cstdio>

// Глобальный флаг инициализации
static bool g_initialized = false;

// Точка входа DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    DisableThreadLibraryCalls(hModule);
    
    if (reason == DLL_PROCESS_ATTACH) {
        // Запуск инициализации в отдельном потоке
        HANDLE hThread = CreateThread(nullptr, 0, 
            [](LPVOID param) -> DWORD {
                HMODULE hMod = static_cast<HMODULE>(param);
                
                // Небольшая задержка для завершения инициализации MTA
                Sleep(3000);
                
                // Инициализация ядра
                if (!swill::HookEngine::Instance().Initialize()) {
                    return 1;
                }
                
                // Обход TamperGuard (netc.dll)
                swill::AntiTamper::DisableTamperLock();
                swill::AntiTamper::PatchTamperHandler();
                swill::AntiTamper::NeutralizeFastFail();
                
                // Обход дополнительных защит
                swill::GuardBypass::Initialize();
                
                // Сетевые хуки (блокировка AC репортов)
                swill::NetworkHooks::Initialize();
                
                // Интеграция с Lua
                swill::LuaIntegration::Initialize();
                
                // Пример выполнения Lua кода
                const char* testCode = R"(
                    outputChatString("[SWILL] Injection successful!", 255, 0, 0)
                )";
                swill::LuaIntegration::ExecuteLua(testCode, "@swill_test");
                
                g_initialized = true;
                
                return 0;
            }, hModule, 0, nullptr);
        
        if (hThread) {
            CloseHandle(hThread);
        }
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (g_initialized) {
            // Очистка при выгрузке
            swill::NetworkHooks::Shutdown();
            swill::HookEngine::Instance().RemoveAllHooks();
        }
    }
    
    return TRUE;
}

// Экспортируемые функции (опционально)
extern "C" {
    __declspec(dllexport) const char* GetSWILLVersion() {
        return "1.0.0";
    }
    
    __declspec(dllexport) bool IsInitialized() {
        return g_initialized;
    }
    
    __declspec(dllexport) bool ExecuteLuaCode(const char* code) {
        if (!g_initialized) return false;
        return swill::LuaIntegration::ExecuteLua(code);
    }
}
