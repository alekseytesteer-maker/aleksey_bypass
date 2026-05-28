#include "core/MemoryUtils.hpp"

namespace swill {

bool MemoryUtils::ProtectMemory(void* address, SIZE_T size, DWORD newProtect, PDWORD oldProtect) {
    if (!address || size == 0)
        return false;

    return VirtualProtect(address, size, newProtect, oldProtect);
}

bool MemoryUtils::WriteMemory(void* address, const void* data, SIZE_T size) {
    if (!address || !data || size == 0)
        return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    memcpy(address, data, size);

    VirtualProtect(address, size, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), address, size);

    return true;
}

bool MemoryUtils::ReadMemory(void* address, void* buffer, SIZE_T size) {
    if (!address || !buffer || size == 0)
        return false;

    memcpy(buffer, address, size);
    return true;
}

bool MemoryUtils::PatchBytes(void* address, const BYTE* bytes, SIZE_T size) {
    return WriteMemory(address, bytes, size);
}

bool MemoryUtils::PatchNop(void* address, SIZE_T count) {
    std::vector<BYTE> nops(count, 0x90);
    return PatchBytes(address, nops.data(), count);
}

bool MemoryUtils::PatchJmp(void* src, void* dst) {
    // JMP rel32: E9 + (dst - src - 5)
    BYTE jmpByte = 0xE9;
    DWORD relAddr = reinterpret_cast<DWORD>(dst) - reinterpret_cast<DWORD>(src) - 5;

    if (!WriteMemory(src, &jmpByte, 1))
        return false;

    return WriteMemory(reinterpret_cast<BYTE*>(src) + 1, &relAddr, sizeof(relAddr));
}

bool MemoryUtils::PatchCall(void* src, void* dst) {
    // CALL rel32: E8 + (dst - src - 5)
    BYTE callByte = 0xE8;
    DWORD relAddr = reinterpret_cast<DWORD>(dst) - reinterpret_cast<DWORD>(src) - 5;

    if (!WriteMemory(src, &callByte, 1))
        return false;

    return WriteMemory(reinterpret_cast<BYTE*>(src) + 1, &relAddr, sizeof(relAddr));
}

void* MemoryUtils::AllocateMemory(SIZE_T size, DWORD allocationType, DWORD protect) {
    return VirtualAlloc(nullptr, size, allocationType, protect);
}

bool MemoryUtils::FreeMemory(void* address) {
    return VirtualFree(address, 0, MEM_RELEASE);
}

bool MemoryUtils::CreateTrampoline(void* src, void* dst, SIZE_T instructionCount) {
    // Создаем трамплин для перехвата функции
    // Копируем оригинальные инструкции в новое место
    // В начале оригинальной функции ставим JMP на нашу функцию

    // Выделяем память для трамплина
    void* trampoline = AllocateMemory(instructionCount + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return false;

    // Копируем оригинальные инструкции
    memcpy(trampoline, src, instructionCount);

    // Добавляем JMP обратно в оригинальную функцию (после наших инструкций)
    void* jmpBackAddr = reinterpret_cast<BYTE*>(src) + instructionCount;
    PatchJmp(reinterpret_cast<BYTE*>(trampoline) + instructionCount, jmpBackAddr);

    // Ставим JMP на нашу функцию в начале оригинальной
    PatchJmp(src, dst);

    return true;
}

} // namespace swill
