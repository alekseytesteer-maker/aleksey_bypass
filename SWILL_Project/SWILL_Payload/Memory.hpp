#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <sstream>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

class SwillMemory {
public:
    // Безопасное наложение патча с восстановлением защиты памяти
    static bool Patch(void* dst, void* src, size_t size) {
        if (!dst || !src || size == 0) return false;
        
        DWORD oldProtect;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }
        
        memcpy(dst, src, size);
        
        // Восстанавливаем оригинальные права доступа
        VirtualProtect(dst, size, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), dst, size);
        
        return true;
    }

    // Заполнение NOP-ами (No Operation)
    static bool Nop(void* dst, size_t size) {
        if (!dst || size == 0) return false;
        
        DWORD oldProtect;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }
        
        memset(dst, 0x90, size);
        
        VirtualProtect(dst, size, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), dst, size);
        
        return true;
    }

    // Запись байт напрямую
    static bool WriteBytes(void* dst, const std::vector<BYTE>& bytes) {
        if (!dst || bytes.empty()) return false;
        return Patch(dst, (void*)bytes.data(), bytes.size());
    }

    // Чтение памяти процесса
    static bool ReadMemory(void* src, void* dst, size_t size) {
        if (!src || !dst || size == 0) return false;
        
        DWORD oldProtect;
        if (!VirtualProtect(src, size, PAGE_READONLY, &oldProtect)) {
            memcpy(dst, src, size);
            return true;
        }
        
        memcpy(dst, src, size);
        VirtualProtect(src, size, oldProtect, &oldProtect);
        
        return true;
    }

    // Движок сканирования сигнатур (AOB Scanner)
    static uintptr_t FindPattern(HMODULE hModule, const std::string& pattern) {
        if (!hModule) return 0;

        MODULEINFO modInfo = { 0 };
        if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO))) {
            return 0;
        }
        
        uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
        uintptr_t size = (uintptr_t)modInfo.SizeOfImage;

        // Парсим сигнатуру типа "55 8B EC ?? 53" в вектор байт и маску
        std::vector<BYTE> patternBytes;
        std::vector<char> mask;
        std::stringstream ss(pattern);
        std::string token;

        while (ss >> token) {
            if (token == "??" || token == "?" || token[0] == '?') {
                patternBytes.push_back(0);
                mask.push_back('?');
            } else {
                try {
                    patternBytes.push_back((BYTE)stoul(token, nullptr, 16));
                    mask.push_back('x');
                } catch (...) {
                    continue;
                }
            }
        }

        if (patternBytes.empty() || patternBytes.size() != mask.size()) {
            return 0;
        }

        // Сканируем память модуля
        for (uintptr_t i = 0; i < size - patternBytes.size(); ++i) {
            bool found = true;
            for (size_t j = 0; j < patternBytes.size(); ++j) {
                if (mask[j] == 'x') {
                    BYTE currentByte = *reinterpret_cast<BYTE*>(base + i + j);
                    if (currentByte != patternBytes[j]) {
                        found = false;
                        break;
                    }
                }
            }
            
            if (found) {
                return base + i; // Возвращаем реальный VA адрес в памяти
            }
        }
        
        return 0;
    }

    // Поиск паттерна в диапазоне адресов
    static uintptr_t FindPatternInRange(uintptr_t start, uintptr_t end, const std::string& pattern) {
        if (start >= end) return 0;

        std::vector<BYTE> patternBytes;
        std::vector<char> mask;
        std::stringstream ss(pattern);
        std::string token;

        while (ss >> token) {
            if (token == "??" || token == "?" || token[0] == '?') {
                patternBytes.push_back(0);
                mask.push_back('?');
            } else {
                try {
                    patternBytes.push_back((BYTE)stoul(token, nullptr, 16));
                    mask.push_back('x');
                } catch (...) {
                    continue;
                }
            }
        }

        if (patternBytes.empty()) return 0;

        for (uintptr_t i = start; i <= end - patternBytes.size(); ++i) {
            bool found = true;
            for (size_t j = 0; j < patternBytes.size(); ++j) {
                if (mask[j] == 'x' && *reinterpret_cast<BYTE*>(i + j) != patternBytes[j]) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return i;
            }
        }

        return 0;
    }

    // Получение адреса функции по имени из модуля
    static FARPROC GetFunctionAddress(HMODULE hModule, const char* funcName) {
        if (!hModule || !funcName) return nullptr;
        return GetProcAddress(hModule, funcName);
    }
};
