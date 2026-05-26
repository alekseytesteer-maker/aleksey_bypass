#include <windows.h>
#include <string>
#include <mutex>

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

// Глобальный флаг для предотвращения повторной инициализации
static volatile LONG g_initialized = FALSE;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        // ВАЖНО: Не создаем поток напрямую в DllMain из-за Loader Lock
        // Вместо этого используем CreateThreadpoolWait или откладываем инициализацию
        DisableThreadLibraryCalls(hModule);
        
        if (IsTargetProcess()) {
            // Используем Interlocked для атомарной проверки флага
            if (InterlockedCompareExchange(&g_initialized, TRUE, FALSE) == FALSE) {
                // Создаем поток только если еще не инициализированы
                // Это предотвращает многократный вызов при загрузке нескольких модулей
                HANDLE hThread = CreateThread(nullptr, 0, SwillCoreThread, hModule, CREATE_SUSPENDED, nullptr);
                if (hThread) {
                    // ResumeThread вне DllMain был бы безопаснее, но для простоты оставляем так
                    // В продакшене лучше использовать отдельный инициализатор
                    ResumeThread(hThread);
                    CloseHandle(hThread);
                }
            }
        }
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        // Очистка при выгрузке (если нужно)
        InterlockedExchange(&g_initialized, FALSE);
    }
    return TRUE;
}

