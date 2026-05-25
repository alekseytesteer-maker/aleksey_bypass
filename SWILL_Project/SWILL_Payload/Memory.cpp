#include "Memory.hpp"

bool SwillMemory::Patch(void* dst, void* src, size_t size) {
    DWORD oldProtect;
    if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    memcpy(dst, src, size);
    
    // Восстанавливаем оригинальные права
    VirtualProtect(dst, size, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), dst, size);
    
    return true;
}

bool SwillMemory::Nop(void* dst, size_t size) {
    DWORD oldProtect;
    if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    memset(dst, 0x90, size);
    
    VirtualProtect(dst, size, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), dst, size);
    
    return true;
}

bool SwillMemory::PatchWithBackup(void* dst, void* src, size_t size, std::vector<BYTE>& backup) {
    // Сохраняем оригинальные байты
    backup.resize(size);
    memcpy(backup.data(), dst, size);
    
    return Patch(dst, src, size);
}

bool SwillMemory::Restore(void* dst, const std::vector<BYTE>& original) {
    if (original.empty()) return false;
    return Patch(dst, (void*)original.data(), original.size());
}

uintptr_t SwillMemory::FindPattern(HMODULE hModule, const char* pattern) {
    if (!hModule) return 0;
    
    MODULEINFO modInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO))) {
        return 0;
    }
    
    uintptr_t base = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
    uintptr_t size = modInfo.SizeOfImage;
    
    return FindPatternInRange(base, size, pattern);
}

uintptr_t SwillMemory::FindPatternInRange(uintptr_t base, size_t size, const char* pattern) {
    if (!base || !size) return 0;
    
    // Парсим сигнатуру типа "55 8B EC ?? 53" в вектор байт и маску
    std::vector<BYTE> patternBytes;
    std::vector<char> mask;
    std::istringstream ss(pattern);
    std::string token;
    
    while (ss >> token) {
        if (token == "??" || token == "?") {
            patternBytes.push_back(0);
            mask.push_back('?');
        } else {
            try {
                patternBytes.push_back(static_cast<BYTE>(std::stoul(token, nullptr, 16)));
                mask.push_back('x');
            } catch (...) {
                continue;
            }
        }
    }
    
    if (patternBytes.empty() || patternBytes.size() > size) {
        return 0;
    }
    
    // Сканируем память модуля с проверкой страниц
    for (size_t i = 0; i <= size - patternBytes.size(); ++i) {
        uintptr_t currentAddress = base + i;
        
        // Проверяем доступность страницы перед чтением
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(currentAddress), &mbi, sizeof(mbi))) {
            continue;
        }
        
        // Пропускаем недоступные страницы
        if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | 
                            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) ||
            (mbi.Protect & PAGE_GUARD)) {
            // Переходим к следующей странице
            i += mbi.RegionSize - 1;
            continue;
        }
        
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (mask[j] == 'x') {
                BYTE currentByte = *reinterpret_cast<BYTE*>(currentAddress + j);
                if (currentByte != patternBytes[j]) {
                    found = false;
                    break;
                }
            }
        }
        
        if (found) {
            return currentAddress;
        }
    }
    
    return 0;
}
