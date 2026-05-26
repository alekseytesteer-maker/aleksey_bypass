#pragma once
#include <windows.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <sstream>

class SwillMemory {
public:
    static bool Patch(void* dst, void* src, size_t size) {
        DWORD oldProtect;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        memcpy(dst, src, size);
        VirtualProtect(dst, size, oldProtect, &oldProtect);
        return true;
    }

    static bool Nop(void* dst, size_t size) {
        DWORD oldProtect;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        memset(dst, 0x90, size);
        VirtualProtect(dst, size, oldProtect, &oldProtect);
        return true;
    }

    static uintptr_t FindPattern(HMODULE hModule, const std::string& pattern) {
        if (!hModule) return 0;

        MODULEINFO modInfo = { 0 };
        if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO))) return 0;
        uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
        uintptr_t size = (uintptr_t)modInfo.SizeOfImage;

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

        for (uintptr_t i = 0; i < size - patternBytes.size(); ++i) {
            bool found = true;
            for (size_t j = 0; j < patternBytes.size(); ++j) {
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

