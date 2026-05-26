#include <windows.h>
#include <string>

extern DWORD WINAPI SwillCoreThread(LPVOID lpParam);

static bool IsTargetProcess() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string path(exePath);
    size_t pos = path.rfind(static_cast<char>(92));
    if (pos == std::string::npos) pos = path.rfind('/');
    std::string exeName = (pos == std::string::npos) ? path : path.substr(pos + 1);
    return (exeName == "gta_sa.exe");
}

// Экспортируемая hook-функция для SetWindowsHookEx
extern "C" __declspec(dllexport) LRESULT CALLBACK SwillHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        if (IsTargetProcess()) {
            CreateThread(nullptr, 0, SwillCoreThread, hModule, 0, nullptr);
        }
    }
    return TRUE;
}

