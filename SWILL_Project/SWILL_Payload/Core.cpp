#include "Memory.hpp"
#include "Hooks.hpp"
#include <windows.h>
#include <psapi.h>
#include <atomic>
#include <cstdio>

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
        return 0; // Безопасный возврат при отсутствии оригинала
    }
    
    uintptr_t isInitAddr = g_CIdArray_IsInitialized.load();
    uintptr_t stackCountAddr = g_CIdArray_IDStackCount.load();
    
    __try {
        // Проверяем инициализацию через безопасное чтение
        char isInit = 0;
        if (!SwillMemory::ReadMemorySafe(isInitAddr, isInit)) {
            // Не можем прочитать - передаем управление оригиналу
            return g_OriginalPopUniqueId(pThis, edx, param_1);
        }
        
        if (!isInit) {
            return g_OriginalPopUniqueId(pThis, edx, param_1);
        }
        
        // Читаем счетчик стека свободных ID
        int stackCount = 0;
        if (!SwillMemory::ReadMemorySafe(stackCountAddr, stackCount)) {
            return g_OriginalPopUniqueId(pThis, edx, param_1);
        }
        
        // Защита от краша при пустом стеке
        if (stackCount <= 0) {
            // Генерируем виртуальный ID вместо краша
            unsigned int capacity = 0;
            uintptr_t baseAddr = g_CIdArray_Base.load();
            if (baseAddr && SwillMemory::ReadMemorySafe(baseAddr, capacity)) {
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
    HMODULE hNetc = nullptr;
    
    // Ждем загрузки netc.dll
    while ((hNetc = GetModuleHandleW(L"netc.dll")) == nullptr) {
        Sleep(200);
    }
    
    // Инициализируем MinHook
    if (!SwillHooks::Initialize()) {
        return 1;
    }
    
    // Находим функцию PopUniqueId по сигнатуре
    uintptr_t funcAddr = SwillMemory::FindPattern(hNetc, SwillShared::Signatures::POP_UNIQUE_ID);
    
    if (!funcAddr) {
        // Функция не найдена - выходим без ошибки
        return 0;
    }
    
    // Вычисляем базовый адрес структуры CIdArray из инструкции A1 ?? ?? ?? ??
    // Смещение 11 байт от начала функции (после prologue)
    uintptr_t globalPtrAddr = *(uintptr_t*)(funcAddr + 11);
    g_CIdArray_Base.store(globalPtrAddr);
    
    // Вычисляем адреса полей структуры
    g_CIdArray_IsInitialized.store(globalPtrAddr + SwillShared::CIdArrayOffsets::IS_INITIALIZED);
    g_CIdArray_IDStackCount.store(globalPtrAddr + SwillShared::CIdArrayOffsets::ID_STACK_COUNT);
    
    // Создаем хук на PopUniqueId
    if (SwillHooks::CreateHook(reinterpret_cast<void*>(funcAddr), 
                               reinterpret_cast<void*>(Hooked_PopUniqueId),
                               reinterpret_cast<void**>(&g_OriginalPopUniqueId))) {
        // Включаем хук
        SwillHooks::EnableHook(reinterpret_cast<void*>(funcAddr));
    }
    
    // Помечаем ядро как готовое
    g_IsCoreReady.store(true);
    
    // Основной цикл ядра (может быть расширен для других задач)
    while (true) {
        Sleep(1000);
        
        // Проверка на команду экстренной выгрузки через IPC могла бы быть здесь
    }
}
