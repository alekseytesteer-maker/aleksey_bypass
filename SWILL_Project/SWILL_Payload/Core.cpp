#include "Memory.hpp"
#include "Hooks.hpp"
#include "Logger.hpp"
#include <iostream>
#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

// Глобальные указатели на оригинальные функции
TPopUniqueId g_OriginalPopUniqueId = nullptr;
TFreeSlot g_OriginalFreeSlot = nullptr;
TReallocate g_OriginalReallocate = nullptr;

// Глобальные адреса структуры CIdArray
uintptr_t g_CIdArray_Base = 0;
uintptr_t g_CIdArray_IsInitialized = 0;
uintptr_t g_CIdArray_Capacity = 0;
uintptr_t g_CIdArray_PopIdCounter = 0;
uintptr_t g_CIdArray_TimeoutLimit = 0;
uintptr_t g_CIdArray_IDStackCapacity = 0;
uintptr_t g_CIdArray_IDStackCount = 0;
uintptr_t g_CIdArray_IDStackData = 0;
uintptr_t g_CIdArray_ArrayData = 0;

// Детур-функция для CIdArray::PopUniqueId (FUN_10241990)
unsigned int __stdcall Hooked_PopUniqueId(void* param_1, void* param_2) {
    // Проверяем инициализацию
    char isInit = *(char*)(g_CIdArray_IsInitialized);
    if (!isInit) {
        SWILL_LOG("CIdArray не инициализирован, вызов оригинальной функции...");
        return g_OriginalPopUniqueId(param_1, param_2);
    }

    // Получаем счетчик свободных ID в стеке
    int stackCount = *(int*)(g_CIdArray_IDStackCount);
    unsigned int capacity = *(unsigned int*)(g_CIdArray_Capacity);

    SWILL_LOG("PopUniqueId вызван: StackCount=" + std::to_string(stackCount) + 
              ", Capacity=" + std::to_string(capacity));

    // Если стек пуст - предотвращаем краш генерацией виртуального ID
    if (stackCount <= 0) {
        SWILL_LOG_WARNING("Стек свободных ID пуст! Генерация виртуального ID...");
        
        // Генерируем безопасный виртуальный индекс за пределами текущей емкости
        unsigned int fakeIndex = capacity + 1000;
        unsigned int encodedId = fakeIndex + 0x2000000;
        
        SWILL_LOG_SUCCESS("Сгенерирован виртуальный ID: 0x" + 
                         std::to_string(encodedId));
        
        return encodedId;
    }

    // Вызываем оригинальную функцию
    return g_OriginalPopUniqueId(param_1, param_2);
}

// Детур-функция для CIdArray::FreeSlot (будет найдена позже)
void __stdcall Hooked_FreeSlot(unsigned int encodedId) {
    SWILL_LOG("FreeSlot вызван для ID: 0x" + std::to_string(encodedId));
    
    if (encodedId < 0x2000000) {
        SWILL_LOG_WARNING("Некорректный ID передан в FreeSlot!");
        return;
    }
    
    // Вызываем оригинальную функцию
    if (g_OriginalFreeSlot) {
        g_OriginalFreeSlot(encodedId);
    }
}

// Поиск всех адресов структуры CIdArray
bool FindCIdArrayAddresses(HMODULE hNetc) {
    SWILL_LOG("Поиск адресов структуры CIdArray...");
    
    // Сигнатура функции PopUniqueId: 55 8B EC 83 EC ?? 53 56 57 A1 ?? ?? ?? ?? 33 C5 89 45 F4 8B 1D
    uintptr_t popUniqueAddr = SwillMemory::FindPattern(hNetc, "55 8B EC 83 EC ?? 53 56 57 A1 ?? ?? ?? ?? 33 C5 89 45 F4 8B 1D");
    
    if (!popUniqueAddr) {
        SWILL_LOG_ERROR("Не удалось найти функцию PopUniqueId!");
        return false;
    }
    
    SWILL_LOG_SUCCESS("Найдена PopUniqueId: 0x" + std::to_string(popUniqueAddr));
    
    // Извлекаем адрес глобального указателя из инструкции A1 ?? ?? ?? ??
    // Формат: A1 [адрес 4 байта]
    uintptr_t globalPtrAddr = *(uintptr_t*)(popUniqueAddr + 11);
    
    if (!globalPtrAddr) {
        SWILL_LOG_ERROR("Не удалось получить адрес глобального указателя CIdArray!");
        return false;
    }
    
    g_CIdArray_Base = globalPtrAddr;
    
    // Расчитываем смещения согласно документации
    // 105c8b58 — m_uiCapacity (смещение +0x00 от базы)
    // 105c8b5c — IsInitialized (смещение +0x04)
    // 105c8b60 — m_uiPopIdCounter (смещение +0x08)
    // 105c8b64 — m_uiTimeoutLimit (смещение +0x0C)
    // 105c8b68 — m_IDStackCapacity (смещение +0x10)
    // 105c8b7c — m_IDStackCount (смещение +0x24)
    // 105c8b80 — m_ArrayData (смещение +0x28)
    
    g_CIdArray_Capacity = g_CIdArray_Base + 0x00;
    g_CIdArray_IsInitialized = g_CIdArray_Base + 0x04;
    g_CIdArray_PopIdCounter = g_CIdArray_Base + 0x08;
    g_CIdArray_TimeoutLimit = g_CIdArray_Base + 0x0C;
    g_CIdArray_IDStackCapacity = g_CIdArray_Base + 0x10;
    g_CIdArray_IDStackCount = g_CIdArray_Base + 0x24;
    g_CIdArray_ArrayData = g_CIdArray_Base + 0x28;
    
    SWILL_LOG_HEX("Базовый адрес CIdArray", g_CIdArray_Base);
    SWILL_LOG_HEX("Адрес m_uiCapacity", g_CIdArray_Capacity);
    SWILL_LOG_HEX("Адрес IsInitialized", g_CIdArray_IsInitialized);
    SWILL_LOG_HEX("Адрес m_IDStackCount", g_CIdArray_IDStackCount);
    SWILL_LOG_HEX("Адрес m_ArrayData", g_CIdArray_ArrayData);
    
    return true;
}

// Поиск функции FreeSlot по паттерну
uintptr_t FindFreeSlotFunction(HMODULE hNetc) {
    SWILL_LOG("Поиск функции CIdArray::FreeSlot...");
    
    // Паттерн для FreeSlot: ищем функцию которая обнуляет ячейки и увеличивает счетчик стека
    // Примерная сигнатура: функция с записью 0 по адресу DAT_105c8b80 + index * 8
    uintptr_t freeSlotAddr = SwillMemory::FindPattern(hNetc, "8B 45 ?? 83 E8 ?? 89 45 ?? 8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 45 ?? 8B 4D ?? 89 04 82 89 54 82 ?? C7 04 82 ?? ?? ?? ?? C7 44 82 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 45 ?? 8B 4D ?? 89 04 82 89 54 82 ?? 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B E5 5D C3");
    
    if (!freeSlotAddr) {
        // Альтернативный паттерн
        freeSlotAddr = SwillMemory::FindPattern(hNetc, "8B 45 ?? 2D ?? ?? ?? ?? 89 45 ?? 8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 45 ?? 8B 4D ?? 89 04 82 89 54 82");
    }
    
    if (freeSlotAddr) {
        SWILL_LOG_SUCCESS("Найдена FreeSlot: 0x" + std::to_string(freeSlotAddr));
    } else {
        SWILL_LOG_WARNING("Не удалось найти FreeSlot по сигнатуре");
    }
    
    return freeSlotAddr;
}

// Поиск ловушки краша и её обезвреживание
bool NeutralizeCrashTrap(HMODULE hNetc) {
    SWILL_LOG("Поиск ловушки краша античита...");
    
    // Сигнатура: C7 05 00 00 00 00 00 00 00 00 (mov dword ptr [0], 0)
    uintptr_t crashTrapAddr = SwillMemory::FindPattern(hNetc, "C7 05 ?? ?? ?? ?? 00 00 00 00");
    
    if (!crashTrapAddr) {
        // Альтернативный поиск точной сигнатуры
        crashTrapAddr = SwillMemory::FindPattern(hNetc, "C7 05 00 00 00 00 00 00 00 00");
    }
    
    if (crashTrapAddr) {
        SWILL_LOG_SUCCESS("Найдена ловушка краша: 0x" + std::to_string(crashTrapAddr));
        
        if (SwillMemory::Nop(reinterpret_cast<void*>(crashTrapAddr), 10)) {
            SWILL_LOG_SUCCESS("Ловушка краша обезврежена (NOP)");
            return true;
        } else {
            SWILL_LOG_ERROR("Не удалось наложить NOP на ловушку краша");
        }
    } else {
        SWILL_LOG_WARNING("Ловушка краша не найдена (возможно уже удалена или изменена)");
    }
    
    return false;
}

// Поиск обработчика сетевых пакетов
uintptr_t FindNetPacketHandler(HMODULE hNetc) {
    SWILL_LOG("Поиск обработчика сетевых пакетов NetBitStream...");
    
    // Сигнатура: 55 8B EC 81 EC 0C 04 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 FC 53 56 57 8B F1
    uintptr_t handlerAddr = SwillMemory::FindPattern(hNetc, "55 8B EC 81 EC 0C 04 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 FC 53 56 57 8B F1");
    
    if (handlerAddr) {
        SWILL_LOG_SUCCESS("Найден обработчик пакетов: 0x" + std::to_string(handlerAddr));
    } else {
        SWILL_LOG_WARNING("Обработчик пакетов не найден");
    }
    
    return handlerAddr;
}

// Основной поток ядра
DWORD WINAPI SwillCoreThread(LPVOID lpParam) {
    HMODULE hModule = (HMODULE)lpParam;
    
    // Инициализация консоли для отладки
    AllocConsole();
    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);
    
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleA("SWILL Core v1.0 - Debug Console");
    
    // Инициализация логгера
    SwillLogger::Initialize("swill_payload.log");
    
    SWILL_LOG("==================================================");
    SWILL_LOG("       SWILL CORE v1.0 | СИГНАТУРНОЕ ЯДРО        ");
    SWILL_LOG("==================================================");
    SWILL_LOG("Запуск ядра...");
    
    // Инициализация MinHook
    if (!SwillHooks::Initialize()) {
        SWILL_LOG_ERROR("Критическая ошибка: не удалось инициализировать MinHook!");
        fclose(stdout);
        FreeConsole();
        return 1;
    }
    
    // Ожидание загрузки netc.dll
    HMODULE hNetc = nullptr;
    SWILL_LOG("Ожидание загрузки netc.dll...");
    
    int waitAttempts = 0;
    while ((hNetc = GetModuleHandleW(L"netc.dll")) == nullptr) {
        Sleep(200);
        waitAttempts++;
        
        if (waitAttempts % 5 == 0) {
            SWILL_LOG("Ожидание netc.dll... (" + std::to_string(waitAttempts * 200) + " мс)");
        }
        
        // Таймаут 30 секунд
        if (waitAttempts > 150) {
            SWILL_LOG_ERROR("netc.dll не загружен в течение 30 секунд!");
            fclose(stdout);
            FreeConsole();
            return 1;
        }
    }
    
    SWILL_LOG_SUCCESS("Модуль netc.dll загружен. Базовый адрес: 0x" + std::to_string((uintptr_t)hNetc));
    
    // Поиск адресов структуры CIdArray
    if (!FindCIdArrayAddresses(hNetc)) {
        SWILL_LOG_ERROR("Не удалось определить адреса CIdArray!");
        // Продолжаем работу без хуков CIdArray
    } else {
        // Установка хука на PopUniqueId
        SWILL_LOG("Установка хука на CIdArray::PopUniqueId...");
        
        uintptr_t popUniqueAddr = SwillMemory::FindPattern(hNetc, "55 8B EC 83 EC ?? 53 56 57 A1 ?? ?? ?? ?? 33 C5 89 45 F4 8B 1D");
        
        if (popUniqueAddr) {
            if (SwillHooks::EnableHook(
                (void*)popUniqueAddr, 
                (void*)Hooked_PopUniqueId, 
                (void**)&g_OriginalPopUniqueId,
                "CIdArray::PopUniqueId")) {
                SWILL_LOG_SUCCESS("Хук на PopUniqueId успешно установлен!");
            } else {
                SWILL_LOG_ERROR("Не удалось установить хук на PopUniqueId");
            }
        }
    }
    
    // Поиск и обезвреживание ловушки краша
    NeutralizeCrashTrap(hNetc);
    
    // Поиск обработчика сетевых пакетов
    uintptr_t packetHandler = FindNetPacketHandler(hNetc);
    if (packetHandler) {
        SWILL_LOG("Обработчик пакетов готов для установки хука (следующий этап)");
    }
    
    // Поиск функции FreeSlot
    uintptr_t freeSlotAddr = FindFreeSlotFunction(hNetc);
    if (freeSlotAddr) {
        SWILL_LOG("Функция FreeSlot найдена, готова для хука");
    }
    
    SWILL_LOG("==================================================");
    SWILL_LOG("Базовое развертывание завершено!");
    SWILL_LOG("Активных хуков: " + std::to_string(SwillHooks::GetHookCount()));
    SWILL_LOG("Система стабильна и готова к работе.");
    SWILL_LOG("==================================================");
    
    // Основной цикл работы ядра
    while (true) {
        Sleep(1000);
        
        // Периодическая проверка состояния
        static int uptimeSeconds = 0;
        uptimeSeconds++;
        
        if (uptimeSeconds % 30 == 0) {
            SWILL_LOG("Uptime: " + std::to_string(uptimeSeconds) + " сек. Хуков активно: " + 
                     std::to_string(SwillHooks::GetHookCount()));
        }
    }
    
    // Очистка (не достижима в нормальном режиме)
    SwillHooks::Shutdown();
    SwillLogger::Shutdown();
    
    if (f) fclose(stdout);
    FreeConsole();
    
    return 0;
}
