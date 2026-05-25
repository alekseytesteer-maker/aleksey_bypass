#pragma once
#include "MinHook/MinHook.h"
#include <iostream>
#include <vector>
#include <string>

// Макрос для удобного логирования
#define SWILL_LOG(msg) std::cout << "[SWILL] " << msg << std::endl
#define SWILL_LOG_HEX(msg, addr) std::cout << "[SWILL] " << msg << " 0x" << std::hex << (uintptr_t)addr << std::dec << std::endl

class SwillHooks {
private:
    static bool s_initialized;
    static int s_hookCount;

public:
    // Инициализация MinHook
    static bool Initialize() {
        if (s_initialized) {
            SWILL_LOG("MinHook уже инициализирован.");
            return true;
        }

        MH_STATUS status = MH_Initialize();
        if (status != MH_OK) {
            std::cerr << "[!] Ошибка инициализации MinHook: " << status << std::endl;
            return false;
        }
        
        s_initialized = true;
        s_hookCount = 0;
        SWILL_LOG("MinHook успешно инициализирован.");
        return true;
    }

    // Деинициализация MinHook
    static void Shutdown() {
        if (!s_initialized) return;
        
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        s_initialized = false;
        s_hookCount = 0;
        
        SWILL_LOG("MinHook деинициализирован.");
    }

    // Создание и включение хука
    static bool EnableHook(void* target, void* detour, void** original, const std::string& hookName = "Unknown") {
        if (!s_initialized) {
            std::cerr << "[!] MinHook не инициализирован!" << std::endl;
            return false;
        }

        if (!target || !detour) {
            std::cerr << "[!] Неверные параметры хука!" << std::endl;
            return false;
        }

        // Создаем хук
        MH_STATUS createStatus = MH_CreateHook(target, detour, original);
        if (createStatus != MH_OK) {
            std::cerr << "[!] Не удалось создать хук '" << hookName << "': " << createStatus << std::endl;
            std::cerr << "    Целевой адрес: 0x" << std::hex << (uintptr_t)target << std::dec << std::endl;
            return false;
        }

        // Включаем хук
        MH_STATUS enableStatus = MH_EnableHook(target);
        if (enableStatus != MH_OK) {
            std::cerr << "[!] Не удалось включить хук '" << hookName << "': " << enableStatus << std::endl;
            MH_RemoveHook(target);
            return false;
        }

        s_hookCount++;
        SWILL_LOG("Хук установлен: " << hookName << " -> 0x" << std::hex << (uintptr_t)target << std::dec);
        
        return true;
    }

    // Отключение конкретного хука
    static bool DisableHook(void* target) {
        if (!s_initialized) return false;
        
        MH_STATUS status = MH_DisableHook(target);
        if (status != MH_OK) {
            std::cerr << "[!] Ошибка отключения хука: " << status << std::endl;
            return false;
        }
        
        s_hookCount--;
        return true;
    }

    // Удаление хука
    static bool RemoveHook(void* target) {
        if (!s_initialized) return false;
        
        MH_STATUS status = MH_RemoveHook(target);
        if (status != MH_OK) {
            std::cerr << "[!] Ошибка удаления хука: " << status << std::endl;
            return false;
        }
        
        s_hookCount--;
        return true;
    }

    // Получение количества активных хуков
    static int GetHookCount() {
        return s_hookCount;
    }

    // Проверка инициализации
    static bool IsInitialized() {
        return s_initialized;
    }
};

// Статические члены класса
inline bool SwillHooks::s_initialized = false;
inline int SwillHooks::s_hookCount = 0;

// Типы функций для хуков CIdArray
typedef unsigned int (__stdcall *TPopUniqueId)(void* param_1, void* param_2);
typedef void (__stdcall *TFreeSlot)(unsigned int encodedId);
typedef bool (__stdcall *TReallocate)(size_t newCapacity);

// Глобальные указатели на оригинальные функции
extern TPopUniqueId g_OriginalPopUniqueId;
extern TFreeSlot g_OriginalFreeSlot;
extern TReallocate g_OriginalReallocate;
