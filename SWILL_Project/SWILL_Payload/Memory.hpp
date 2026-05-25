// SWILL Payload - Memory Scanner & Patch Engine
#pragma once

#include <windows.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

#pragma comment(lib, "psapi.lib")

class SwillMemory {
public:
    // Safe memory patching with protection restoration
    static bool Patch(void* dst, void* src, size_t size) {
        DWORD oldProtect;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) 
            return false;
        memcpy(dst, src, size);
        VirtualProtect(dst, size, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), dst, size);
        return true;
    }

    // NOP fill
    static bool Nop(void* dst, size_t size) {
        DWORD oldProtect;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) 
            return false;
        memset(dst, 0x90, size);
        VirtualProtect(dst, size, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), dst, size);
        return true;
    }

    // AOB Pattern Scanner
    static uintptr_t FindPattern(HMODULE hModule, const std::string& pattern) {
        if (!hModule) return 0;

        MODULEINFO modInfo;
        if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO)))
            return 0;

        uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
        uintptr_t size = (uintptr_t)modInfo.SizeOfImage;

        // Parse pattern
        std::vector<BYTE> patternBytes;
        std::vector<char> mask;
        std::stringstream ss(pattern);
        std::string token;

        while (ss >> token) {
            if (token == "??" || token == "?") {
                patternBytes.push_back(0);
                mask.push_back('?');
            } else {
                patternBytes.push_back((BYTE)std::stoul(token, nullptr, 16));
                mask.push_back('x');
            }
        }

        if (patternBytes.empty()) return 0;

        // Scan memory
        for (uintptr_t i = 0; i < size - patternBytes.size(); ++i) {
            bool found = true;
            for (size_t j = 0; j < patternBytes.size(); ++j) {
                if (mask[j] == 'x' && 
                    *reinterpret_cast<BYTE*>(base + i + j) != patternBytes[j]) {
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

    // Find pattern in specific module by name
    static uintptr_t FindPatternInModule(const char* moduleName, const std::string& pattern) {
        HMODULE hMod = GetModuleHandleA(moduleName);
        if (!hMod) return 0;
        return FindPattern(hMod, pattern);
    }

    // Read memory safely
    static bool ReadMemory(void* address, void* buffer, size_t size) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(address, &mbi, sizeof(mbi))) return false;
        
        if (mbi.State != MEM_COMMIT) return false;
        if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
            return false;

        memcpy(buffer, address, size);
        return true;
    }

    // Write memory safely
    static bool WriteMemory(void* address, void* buffer, size_t size) {
        DWORD oldProtect;
        if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;
        
        memcpy(address, buffer, size);
        FlushInstructionCache(GetCurrentProcess(), address, size);
        
        VirtualProtect(address, size, oldProtect, &oldProtect);
        return true;
    }
};
