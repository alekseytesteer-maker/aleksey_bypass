// SWILL Payload - DLL Entry Point
#include <windows.h>
#include <iostream>

// Forward declaration of core thread
DWORD WINAPI SwillCoreThread(LPVOID lpParam);

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            
            // Create core thread for payload execution
            HANDLE hThread = CreateThread(nullptr, 0, SwillCoreThread, hModule, 0, nullptr);
            if (hThread) {
                CloseHandle(hThread);
            } else {
                return FALSE;
            }
            break;
        }
        case DLL_PROCESS_DETACH:
            // Cleanup handled by core thread shutdown
            break;
    }
    return TRUE;
}
