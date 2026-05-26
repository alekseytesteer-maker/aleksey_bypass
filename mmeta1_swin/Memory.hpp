#pragma once
#include <windows.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <sstream>

class SwillMemory {
public:
    static bool Patch(void* dst, void* src, size_t size) {
        if (!dst || !src || size == 0) return false;
        
        // Проверяем текущую защиту памяти
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(dst, &mbi, sizeof(mbi))) return false;
        
        // Если память уже имеет нужные права, патчим напрямую
        if (mbi.Protect & (PAGE_EXECUTE_READWRITE | PAGE_READWRITE)) {
            memcpy(dst, src, size);
            return true;
        }
        
        // Иначе меняем защиту
        DWORD oldProtect;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }
        memcpy(dst, src, size);
        // Восстанавливаем оригинальную защиту (важно для скрытности!)
        VirtualProtect(dst, size, oldProtect, &oldProtect);
        // Сбрасываем кэш инструкций процессора
        FlushInstructionCache(GetCurrentProcess(), dst, size);
        return true;
    }

    static bool Nop(void* dst, size_t size) {
        if (!dst || size == 0) return false;
        
        // Проверяем текущую защиту памяти
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(dst, &mbi, sizeof(mbi))) return false;
        
        // Если память уже имеет нужные права, патчим напрямую
        if (mbi.Protect & (PAGE_EXECUTE_READWRITE | PAGE_READWRITE)) {
            memset(dst, 0x90, size);
            return true;
        }
        
        // Иначе меняем защиту
        DWORD oldProtect;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }
        memset(dst, 0x90, size);
        // Восстанавливаем оригинальную защиту (важно для скрытности!)
        VirtualProtect(dst, size, oldProtect, &oldProtect);
        // Сбрасываем кэш инструкций процессора
        FlushInstructionCache(GetCurrentProcess(), dst, size);
        return true;
    }

    static uintptr_t FindPattern(HMODULE hModule, const std::string& pattern) {
        if (!hModule) return 0;

        MODULEINFO modInfo = { 0 };
        if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO))) return 0;
        uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
        uintptr_t size = (uintptr_t)modInfo.SizeOfImage;

        // Оптимизация: парсим маску один раз перед сканированием, а не в цикле
        std::vector<BYTE> patternBytes;
        std::vector<char> mask;
        std::stringstream ss(pattern);
        std::string token;

        while (ss >> token) {
            if (token == "??" || token == "?") {
                patternBytes.push_back(0);
                mask.push_back('?');
            } else {
                patternBytes.push_back((BYTE)stoul(token, nullptr, 16));
                mask.push_back('x');
            }
        }

        if (patternBytes.empty()) return 0;

        // Оптимизированный поиск с минимальным количеством операций в цикле
        const size_t patternSize = patternBytes.size();
        for (uintptr_t i = 0; i < size - patternSize; ++i) {
            bool found = true;
            for (size_t j = 0; j < patternSize; ++j) {
                if (mask[j] == 'x' && *reinterpret_cast<BYTE*>(base + i + j) != patternBytes[j]) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return base + i;
            }
        }
        return 0;
    }
};

