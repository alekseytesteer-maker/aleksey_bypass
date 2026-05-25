#pragma once
#include <windows.h>
#include <vector>
#include <mutex>
#include <atomic>
#include "../3rdparty/MinHook/include/MinHook.h"

// Структура для хранения информации о хуке
struct HookInfo {
    void* target;
    void* detour;
    void* original;
    bool isEnabled;
    std::vector<BYTE> originalBytes; // Для восстановления
};

class SwillHooks {
public:
    static bool Initialize();
    static void Shutdown();
    
    // Создание inline-хука через MinHook
    static bool CreateHook(void* target, void* detour, void** original);
    
    // Снятие хука с восстановлением оригинальных байтов
    static bool RemoveHook(void* target);
    
    // Включение/выключение хука
    static bool EnableHook(void* target);
    static bool DisableHook(void* target);
    
    // Проверка состояния
    static bool IsInitialized();
    static size_t GetActiveHookCount();
    
private:
    static std::vector<HookInfo> s_hooks;
    static std::mutex s_mutex;
    static std::atomic<bool> s_initialized;
};
