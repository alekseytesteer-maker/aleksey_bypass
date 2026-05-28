#include "core/PatternScanner.hpp"
#include <Psapi.h>

namespace swill {

bool PatternScanner::ScanPattern(const void* base, size_t size, const BYTE* pattern, const char* mask, void** outAddress) {
    if (!base || !pattern || !mask || !outAddress)
        return false;

    const BYTE* buffer = static_cast<const BYTE*>(base);
    size_t patternLen = strlen(mask);

    for (size_t i = 0; i <= size - patternLen; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (mask[j] != '?' && pattern[j] != buffer[i + j]) {
                found = false;
                break;
            }
        }

        if (found) {
            *outAddress = const_cast<BYTE*>(&buffer[i]);
            return true;
        }
    }

    return false;
}

bool PatternScanner::FindPatternInModule(HMODULE module, const char* moduleName, const BYTE* pattern, const char* mask, void** outAddress) {
    if (!module || !pattern || !mask || !outAddress)
        return false;

    MODULEINFO modInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), module, &modInfo, sizeof(modInfo)))
        return false;

    return ScanPattern(modInfo.lpBaseOfDll, modInfo.SizeOfImage, pattern, mask, outAddress);
}

bool PatternScanner::FindPatternInProcess(DWORD processId, HMODULE module, const BYTE* pattern, const char* mask, void** outAddress) {
    if (!processId || !module || !pattern || !mask || !outAddress)
        return false;

    HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (!hProcess)
        return false;

    MODULEINFO modInfo = {};
    if (!GetModuleInformation(hProcess, module, &modInfo, sizeof(modInfo))) {
        CloseHandle(hProcess);
        return false;
    }

    // Для удаленного процесса нужно читать память
    std::vector<BYTE> buffer(modInfo.SizeOfImage);
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(hProcess, modInfo.lpBaseOfDll, buffer.data(), buffer.size(), &bytesRead)) {
        CloseHandle(hProcess);
        return false;
    }

    CloseHandle(hProcess);

    void* localAddr = nullptr;
    if (ScanPattern(buffer.data(), bytesRead, pattern, mask, &localAddr)) {
        size_t offset = reinterpret_cast<BYTE*>(localAddr) - buffer.data();
        *outAddress = reinterpret_cast<BYTE*>(modInfo.lpBaseOfDll) + offset;
        return true;
    }

    return false;
}

std::vector<void*> PatternScanner::FindAllPatterns(const void* base, size_t size, const BYTE* pattern, const char* mask) {
    std::vector<void*> addresses;

    if (!base || !pattern || !mask)
        return addresses;

    const BYTE* buffer = static_cast<const BYTE*>(base);
    size_t patternLen = strlen(mask);

    for (size_t i = 0; i <= size - patternLen; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (mask[j] != '?' && pattern[j] != buffer[i + j]) {
                found = false;
                break;
            }
        }

        if (found) {
            addresses.push_back(const_cast<BYTE*>(&buffer[i]));
        }
    }

    return addresses;
}

} // namespace swill
