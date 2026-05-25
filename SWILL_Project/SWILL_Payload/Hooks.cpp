#include "Hooks.hpp"
#include <algorithm>

// Статические члены класса
std::vector<HookInfo> SwillHooks::s_hooks;
std::mutex SwillHooks::s_mutex;
std::atomic<bool> SwillHooks::s_initialized(false);

bool SwillHooks::Initialize() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_initialized.load()) {
        return true; // Уже инициализировано
    }
    
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        return false;
    }
    
    s_initialized.store(true);
    return true;
}

void SwillHooks::Shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (!s_initialized.load()) {
        return;
    }
    
    // Снимаем все хуки
    for (auto& hook : s_hooks) {
        if (hook.isEnabled) {
            MH_DisableHook(hook.target);
        }
        MH_RemoveHook(hook.target);
    }
    
    s_hooks.clear();
    MH_Uninitialize();
    s_initialized.store(false);
}

bool SwillHooks::CreateHook(void* target, void* detour, void** original) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (!s_initialized.load()) {
        return false;
    }
    
    // Проверяем не создан ли уже хук на эту функцию
    for (const auto& hook : s_hooks) {
        if (hook.target == target) {
            return false; // Хук уже существует
        }
    }
    
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        return false;
    }
    
    // Сохраняем информацию о хуке
    HookInfo info;
    info.target = target;
    info.detour = detour;
    info.original = *original;
    info.isEnabled = false;
    
    s_hooks.push_back(info);
    return true;
}

bool SwillHooks::RemoveHook(void* target) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = std::find_if(s_hooks.begin(), s_hooks.end(),
        [target](const HookInfo& hook) { return hook.target == target; });
    
    if (it == s_hooks.end()) {
        return false; // Хук не найден
    }
    
    if (it->isEnabled) {
        MH_DisableHook(target);
    }
    
    MH_STATUS status = MH_RemoveHook(target);
    if (status != MH_OK) {
        return false;
    }
    
    s_hooks.erase(it);
    return true;
}

bool SwillHooks::EnableHook(void* target) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = std::find_if(s_hooks.begin(), s_hooks.end(),
        [target](const HookInfo& hook) { return hook.target == target; });
    
    if (it == s_hooks.end()) {
        return false;
    }
    
    if (it->isEnabled) {
        return true; // Уже включен
    }
    
    MH_STATUS status = MH_EnableHook(target);
    if (status != MH_OK) {
        return false;
    }
    
    it->isEnabled = true;
    return true;
}

bool SwillHooks::DisableHook(void* target) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = std::find_if(s_hooks.begin(), s_hooks.end(),
        [target](const HookInfo& hook) { return hook.target == target; });
    
    if (it == s_hooks.end()) {
        return false;
    }
    
    if (!it->isEnabled) {
        return true; // Уже выключен
    }
    
    MH_STATUS status = MH_DisableHook(target);
    if (status != MH_OK) {
        return false;
    }
    
    it->isEnabled = false;
    return true;
}

bool SwillHooks::IsInitialized() {
    return s_initialized.load();
}

size_t SwillHooks::GetActiveHookCount() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    size_t count = 0;
    for (const auto& hook : s_hooks) {
        if (hook.isEnabled) {
            count++;
        }
    }
    return count;
}

// Экспортная функция для вызова из dllmain
extern "C" void __stdcall SwillHooks_Shutdown_Export() {
    SwillHooks::Shutdown();
}
