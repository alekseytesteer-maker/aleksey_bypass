#define _CRT_SECURE_NO_WARNINGS
#include "Memory.hpp"
#include "Hooks.hpp"
#include "../SWILL_Shared/SharedDefs.hpp"
#include <windows.h>
#include <psapi.h>
#include <atomic>
#include <cstdio>
#include <iostream>

#pragma comment(lib, "psapi.lib")

// Глобальные атомарные переменные для потокобезопасности
std::atomic<uintptr_t> g_CIdArray_Base(0);
std::atomic<uintptr_t> g_CIdArray_IsInitialized(0);
std::atomic<uintptr_t> g_CIdArray_IDStackCount(0);
std::atomic<bool> g_IsCoreReady(false);

// Тип оригинальной функции PopUniqueId (__fastcall для эмуляции __thiscall)
typedef unsigned int (__fastcall *TPopUniqueId)(void* pThis, void* edx, void* param_1);
TPopUniqueId g_OriginalPopUniqueId = nullptr;

// Детур-функция для CIdArray::PopUniqueId (FUN_10241990)
// Используем __fastcall для корректного перехвата __thiscall в x86
unsigned int __fastcall Hooked_PopUniqueId(void* pThis, void* edx, void* param_1) {
    // Проверка на валидность оригинального указателя
    if (!g_OriginalPopUniqueId) {
        return 0x20000001; // Безопасный возврат при отсутствии оригинала
    }
    
    uintptr_t isInitAddr = g_CIdArray_IsInitialized.load();
    uintptr_t stackCountAddr = g_CIdArray_IDStackCount.load();
    
    __try {
        // Проверяем инициализацию через безопасное чтение
        char isInit = 0;
        if (isInitAddr && !SwillMemory::ReadMemorySafe((LPCVOID)isInitAddr, &isInit, sizeof(char))) {
            // Не можем прочитать - передаем управление оригиналу
            return g_OriginalPopUniqueId(pThis, edx, param_1);
        }
        
        if (!isInit) {
            return g_OriginalPopUniqueId(pThis, edx, param_1);
        }
        
        // Читаем счетчик стека свободных ID
        int stackCount = 0;
        if (stackCountAddr && !SwillMemory::ReadMemorySafe((LPCVOID)stackCountAddr, &stackCount, sizeof(int))) {
            return g_OriginalPopUniqueId(pThis, edx, param_1);
        }
        
        // Защита от краша при пустом стеке
        if (stackCount <= 0) {
            // Генерируем виртуальный ID вместо краша
            unsigned int capacity = 0;
            uintptr_t baseAddr = g_CIdArray_Base.load();
            if (baseAddr && SwillMemory::ReadMemorySafe((LPCVOID)baseAddr, &capacity, sizeof(unsigned int))) {
                unsigned int fakeIndex = capacity + 1000;
                return fakeIndex + 0x2000000;
            }
            // Fallback: просто возвращаем безопасное значение
            return 0x20000001;
        }
        
        // Все проверки пройдены - вызываем оригинал
        return g_OriginalPopUniqueId(pThis, edx, param_1);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        // При любом исключении возвращаем безопасное значение
        return 0x20000001;
    }
}

DWORD WINAPI SwillCoreThread(LPVOID lpParam) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    
    std::cout << "==================================================" << std::endl;
    std::cout << "     SWILL CORE v" SWILL_VERSION " | СИГНАТУРНЫЙ МОНИТОР" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    HMODULE hNetc = nullptr;
    
    // Ждем загрузки netc.dll
    std::cout << "[*] Waiting for netc.dll..." << std::endl;
    while ((hNetc = GetModuleHandleW(L"netc.dll")) == nullptr) {
        Sleep(200);
    }
    
    std::cout << "[+] netc.dll loaded at: 0x" << std::hex << (uintptr_t)hNetc << std::endl;
    
    // Инициализируем MinHook
    if (!SwillHooks::Initialize()) {
        std::cout << "[!] MinHook initialization failed!" << std::endl;
        fclose(stdout);
        FreeConsole();
        return 1;
    }
    
    std::cout << "[+] MinHook initialized" << std::endl;
    
    // Находим функцию PopUniqueId по сигнатуре
    std::cout << "[*] Scanning for PopUniqueId signature..." << std::endl;
    uintptr_t funcAddr = SwillMemory::FindPattern(hNetc, SwillShared::Signatures::POP_UNIQUE_ID);
    
    if (!funcAddr) {
        std::cout << "[!] PopUniqueId signature not found!" << std::endl;
        fclose(stdout);
        FreeConsole();
        return 0;
    }
    
    std::cout << "[+] PopUniqueId found at: 0x" << std::hex << funcAddr << std::endl;
    
    // Вычисляем базовый адрес структуры CIdArray из инструкции A1 ?? ?? ?? ??
    // Смещение 11 байт от начала функции (после prologue)
    uintptr_t globalPtrAddr = *(uintptr_t*)(funcAddr + 11);
    g_CIdArray_Base.store(globalPtrAddr);
    
    // Вычисляем адреса полей структуры
    g_CIdArray_IsInitialized.store(globalPtrAddr + SwillShared::CIdArrayOffsets::IS_INITIALIZED);
    g_CIdArray_IDStackCount.store(globalPtrAddr + SwillShared::CIdArrayOffsets::ID_STACK_COUNT);
    
    std::cout << "[->] CIdArray Base: 0x" << std::hex << globalPtrAddr << std::endl;
    std::cout << "[->] IsInitialized at: 0x" << (globalPtrAddr + SwillShared::CIdArrayOffsets::IS_INITIALIZED) << std::endl;
    std::cout << "[->] IDStackCount at: 0x" << (globalPtrAddr + SwillShared::CIdArrayOffsets::ID_STACK_COUNT) << std::endl;
    
    // Создаем хук на PopUniqueId
    std::cout << "[*] Installing hook on PopUniqueId..." << std::endl;
    if (SwillHooks::CreateHook(reinterpret_cast<void*>(funcAddr), 
                               reinterpret_cast<void*>(Hooked_PopUniqueId),
                               reinterpret_cast<void**>(&g_OriginalPopUniqueId))) {
        // Включаем хук
        if (SwillHooks::EnableHook(reinterpret_cast<void*>(funcAddr))) {
            std::cout << "[+++++] Hook installed successfully!" << std::endl;
            std::cout << "[*] CIdArray crash protection active" << std::endl;
        } else {
            std::cout << "[!] Failed to enable hook" << std::endl;
        }
    } else {
        std::cout << "[!] Failed to create hook" << std::endl;
    }
    
    // Поиск обработчика пакетов NetBitStream
    std::cout << "[*] Scanning for NetBitStream handler..." << std::endl;
    uintptr_t packetHandler = SwillMemory::FindPattern(hNetc, SwillShared::Signatures::NET_PACKET_HANDLER);
    if (packetHandler) {
        std::cout << "[+] NetBitStream handler found at: 0x" << std::hex << packetHandler << std::endl;
        std::cout << "[*] Ready for packet interception (future feature)" << std::endl;
    } else {
        std::cout << "[!] NetBitStream handler not found" << std::endl;
    }
    
    // Помечаем ядро как готовое
    g_IsCoreReady.store(true);
    
    std::cout << "==================================================" << std::endl;
    std::cout << "[*] SWILL Core is running. Press INSERT for GUI." << std::endl;
    std::cout << "==================================================" << std::endl;
    
    // Основной цикл ядра
    while (true) {
        Sleep(1000);
    }
}
