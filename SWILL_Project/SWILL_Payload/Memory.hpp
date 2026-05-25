#pragma once
#include <windows.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <sstream>
#include "../SWILL_Shared/SharedDefs.hpp"

class SwillMemory {
public:
    // Безопасное наложение патча с сохранением оригинальных прав
    static bool Patch(void* dst, void* src, size_t size);
    
    // Заполнение NOP-ами
    static bool Nop(void* dst, size_t size);
    
    // Сохранение оригинальных байтов для последующего восстановления
    static bool PatchWithBackup(void* dst, void* src, size_t size, std::vector<BYTE>& backup);
    
    // Восстановление оригинальных байтов
    static bool Restore(void* dst, const std::vector<BYTE>& original);
    
    // Безопасное чтение памяти с проверкой страниц
    template<typename T>
    static bool ReadMemorySafe(uintptr_t address, T& outValue) {
        __try {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi))) {
                return false;
            }
            
            // Проверяем что страница доступна для чтения
            if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | 
                                 PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) ||
                (mbi.Protect & PAGE_GUARD)) {
                return false;
            }
            
            outValue = *reinterpret_cast<T*>(address);
            return true;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
    
    // Движок сканирования сигнатур (AOB Scanner) с защитой от невалидных страниц
    static uintptr_t FindPattern(HMODULE hModule, const char* pattern);
    
    // Сканирование конкретного диапазона памяти
    static uintptr_t FindPatternInRange(uintptr_t base, size_t size, const char* pattern);
};
