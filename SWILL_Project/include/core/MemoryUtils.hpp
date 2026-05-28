#pragma once

#include "shared/SharedDefs.hpp"

namespace swill {

inline bool MemoryUtils::WriteMemory(void* address, const void* data, size_t size) {
    if (!address || !data || size == 0) return false;
    
    DWORD oldProtect;
    if (VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(address, data, size);
        VirtualProtect(address, size, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), address, size);
        return true;
    }
    return false;
}

inline bool MemoryUtils::WriteNop(void* address, size_t count) {
    if (!address || count == 0) return false;
    
    std::vector<uint8_t> nops(count, 0x90);
    return WriteMemory(address, nops.data(), count);
}

inline bool MemoryUtils::PatchBytes(void* address, const std::vector<uint8_t>& bytes) {
    if (!address || bytes.empty()) return false;
    return WriteMemory(address, bytes.data(), bytes.size());
}

inline bool MemoryUtils::ReadProcessMemory(HANDLE hProcess, void* address, 
                                            void* buffer, size_t size) {
    if (!hProcess || !address || !buffer || size == 0) return false;
    
    SIZE_T bytesRead = 0;
    return ::ReadProcessMemory(hProcess, address, buffer, size, &bytesRead) != FALSE 
           && bytesRead == size;
}

inline bool MemoryUtils::WriteProcessMemory(HANDLE hProcess, void* address, 
                                             const void* buffer, size_t size) {
    if (!hProcess || !address || !buffer || size == 0) return false;
    
    DWORD oldProtect;
    SIZE_T bytesWritten = 0;
    
    // Виртуальная защита не нужна для WriteProcessMemory, но полезна для локальной памяти
    return ::WriteProcessMemory(hProcess, address, buffer, size, &bytesWritten) != FALSE 
           && bytesWritten == size;
}

inline void* MemoryUtils::AllocateMemory(HANDLE hProcess, size_t size) {
    if (!hProcess || size == 0) return nullptr;
    return VirtualAllocEx(hProcess, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

inline bool MemoryUtils::FreeMemory(HANDLE hProcess, void* address) {
    if (!hProcess || !address) return false;
    return VirtualFreeEx(hProcess, address, 0, MEM_RELEASE) != FALSE;
}

} // namespace swill
