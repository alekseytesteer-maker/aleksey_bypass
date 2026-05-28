#pragma once

#include "shared/SharedDefs.hpp"

namespace swill {

inline void* PatternScanner::FindPattern(const char* moduleName, const char* pattern, const char* mask) {
    HMODULE hModule = GetModuleHandleA(moduleName);
    if (!hModule) return nullptr;
    
    IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
    IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(hModule) + dosHeader->e_lfanew);
    
    size_t moduleSize = ntHeaders->OptionalHeader.SizeOfImage;
    
    return FindPatternInRange(hModule, moduleSize, pattern, mask);
}

inline void* PatternScanner::FindPatternInRange(void* start, size_t size, 
                                                 const char* pattern, const char* mask) {
    uint8_t* begin = static_cast<uint8_t*>(start);
    
    for (size_t i = 0; i < size; ++i) {
        if (CompareBytes(begin + i, reinterpret_cast<const uint8_t*>(pattern), mask, strlen(mask))) {
            return begin + i;
        }
    }
    
    return nullptr;
}

inline void* PatternScanner::GetExportAddress(HMODULE module, const char* functionName) {
    if (!module || !functionName) return nullptr;
    
    IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(module) + dosHeader->e_lfanew);
    
    IMAGE_DATA_DIRECTORY exportDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!exportDir.VirtualAddress) return nullptr;
    
    IMAGE_EXPORT_DIRECTORY* exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
        reinterpret_cast<uint8_t*>(module) + exportDir.VirtualAddress);
    
    uint32_t* names = reinterpret_cast<uint32_t*>(
        reinterpret_cast<uint8_t*>(module) + exports->AddressOfNames);
    uint16_t* ordinals = reinterpret_cast<uint16_t*>(
        reinterpret_cast<uint8_t*>(module) + exports->AddressOfNameOrdinals);
    uint32_t* functions = reinterpret_cast<uint32_t*>(
        reinterpret_cast<uint8_t*>(module) + exports->AddressOfFunctions);
    
    for (uint32_t i = 0; i < exports->NumberOfNames; ++i) {
        const char* name = reinterpret_cast<const char*>(
            reinterpret_cast<uint8_t*>(module) + names[i]);
        
        if (_stricmp(name, functionName) == 0) {
            uint16_t ordinal = ordinals[i];
            return reinterpret_cast<void*>(
                reinterpret_cast<uint8_t*>(module) + functions[ordinal]);
        }
    }
    
    return nullptr;
}

inline bool PatternScanner::CompareBytes(const uint8_t* data, const uint8_t* pattern, 
                                          const char* mask, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (mask[i] == 'x' && data[i] != pattern[i]) {
            return false;
        }
    }
    return true;
}

} // namespace swill
