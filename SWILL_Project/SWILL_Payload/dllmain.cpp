#include <windows.h>

// Объявление потока ядра из Core.cpp
extern DWORD WINAPI SwillCoreThread(LPVOID lpParam);

// Объявление функции Shutdown из Hooks.cpp
extern "C" void __stdcall SwillHooks_Shutdown_Export();

// Глобальный флаг для отслеживания состояния
static bool g_isInitialized = false;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    // Отключаем уведомления о потоках для производительности
    DisableThreadLibraryCalls(hModule);
    
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            if (g_isInitialized) break;
            g_isInitialized = true;
            
            // Создаем поток ядра в отдельном потоке
            HANDLE hThread = CreateThread(nullptr, 0, SwillCoreThread, hModule, 0, nullptr);
            if (hThread) {
                CloseHandle(hThread); // Закрываем дескриптор, поток продолжит работу
            }
            break;
        }
        
        case DLL_PROCESS_DETACH: {
            if (!g_isInitialized) break;
            
            // Безопасная выгрузка: снимаем все хуки перед выходом
            // Это предотвращает краш игры при выгрузке DLL
            SwillHooks_Shutdown_Export();
            
            g_isInitialized = false;
            break;
        }
        
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    
    return TRUE;
}
