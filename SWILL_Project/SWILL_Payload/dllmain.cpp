#include <windows.h>

// Объявление функции ядра
extern DWORD WINAPI SwillCoreThread(LPVOID lpParam);

// Точка входа DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            // Отключаем уведомления о потоках для производительности
            DisableThreadLibraryCalls(hModule);
            
            // Создаем поток ядра
            HANDLE hThread = CreateThread(nullptr, 0, SwillCoreThread, hModule, 0, nullptr);
            
            if (!hThread) {
                MessageBoxA(NULL, "Не удалось создать поток SWILL Core!", "SWILL Error", MB_ICONERROR);
                return FALSE;
            }
            
            // Закрываем handle потока (поток продолжит работать)
            CloseHandle(hThread);
            
            break;
        }
        
        case DLL_PROCESS_DETACH: {
            // Очистка при выгрузке DLL
            // Здесь можно добавить дополнительную логику очистки
            break;
        }
        
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    
    return TRUE;
}
