#pragma once

#include "shared/SharedDefs.hpp"

namespace swill {

// Обход TamperGuard из netc.dll
// Основано на анализе: защита через XOR-пары и флаг g_tamper_lock

inline bool AntiTamper::DisableTamperLock() {
    // Сигнатура для поиска g_tamper_lock в netc.dll
    // 55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 83 EC ? A1 ? ? ? ? 33 C5 89 45 ? 56 50 8D 45 ? 64 A3 ? ? ? ? C7 45 ? ? ? ? ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ?
    PatternScanner scanner;
    
    void* netcBase = GetModuleHandleA("netc.dll");
    if (!netcBase) {
        netcBase = GetModuleHandleA("netc"); // Альтернативное имя
    }
    
    if (!netcBase) return false;
    
    // Ищем функцию с проверкой TamperGuard (FUN_1026A540 или аналог)
    // Сигнатура основана на анализе: сравнение this-pointer с зашифрованным эталоном
    const char* sig = "\x80\x3D\x00\x00\x00\x00\x00\x0F\x85";
    const char* mask = "xx????xxx";
    
    void* addr = scanner.FindPatternInRange(netcBase, 0x100000, sig, mask);
    if (!addr) return false;
    
    // По адресу смещения +2 находится указатель на g_tamper_lock
    uint8_t* bytePtr = static_cast<uint8_t*>(addr);
    g_tamperLockAddr = *reinterpret_cast<void**>(bytePtr + 2);
    
    if (g_tamperLockAddr) {
        // Устанавливаем флаг в false (0)
        uint8_t zero = 0;
        return MemoryUtils::WriteMemory(g_tamperLockAddr, &zero, sizeof(zero));
    }
    
    return false;
}

inline bool AntiTamper::PatchTamperHandler() {
    // Патч функции FUN_1026A540 (обработчик нарушений целостности)
    // Заменяем первые инструкции на RET (C3) чтобы отключить обработку
    
    PatternScanner scanner;
    void* netcBase = GetModuleHandleA("netc.dll");
    if (!netcBase) netcBase = GetModuleHandleA("netc");
    if (!netcBase) return false;
    
    // Ищем INT 0x29 (__fastfail) который вызывается при нарушении
    // CD 29 или F1 (INT1 как fastfail в некоторых билдах)
    const char* sig = "\xCD\x29";
    const char* mask = "xx";
    
    void* addr = scanner.FindPatternInRange(netcBase, 0x100000, sig, mask);
    if (!addr) {
        // Пробуем альтернативную сигнатуру с __fastfail
        const char* sig2 = "\xF1"; // INT1
        addr = scanner.FindPatternInRange(netcBase, 0x100000, sig2, "x");
    }
    
    if (addr) {
        // Заменяем INT 0x29 на NOP
        return MemoryUtils::WriteNop(addr, 1);
    }
    
    return false;
}

inline bool AntiTamper::BypassGuards() {
    // Обход guard-функций FUN_1026A4B0 и FUN_1026A520
    // Эти функции проверяют XOR-пары объектов
    
    PatternScanner scanner;
    void* netcBase = GetModuleHandleA("netc.dll");
    if (!netcBase) netcBase = GetModuleHandleA("netc");
    if (!netcBase) return false;
    
    // Ищем паттерн XOR-сравнения: 33 D0 / 33 C0 / 31 C0 / 31 D0 и т.д.
    // followed by test/jz
    
    // Упрощённый подход: ищем последовательность XOR reg, reg
    const char* sig = "\x33\xC0\x33\xD0";
    const char* mask = "xxxx";
    
    void* addr = scanner.FindPatternInRange(netcBase, 0x100000, sig, mask);
    if (addr) {
        // Заменяем XOR на NOPs
        return MemoryUtils::WriteNop(addr, 4);
    }
    
    return false;
}

inline bool AntiTamper::NeutralizeFastFail() {
    // Поиск и нейтрализация всех __fastfail вызовов в netc.dll
    
    PatternScanner scanner;
    void* netcBase = GetModuleHandleA("netc.dll");
    if (!netcBase) netcBase = GetModuleHandleA("netc");
    if (!netcBase) return false;
    
    // __fastfail генерирует инструкцию:
    // CD 29 (INT 0x29) или F1 (INT1)
    // Иногда: CALL __fastfail (вызов функции)
    
    int patchedCount = 0;
    size_t offset = 0;
    
    while (offset < 0x100000) {
        uint8_t* base = static_cast<uint8_t*>(netcBase) + offset;
        
        // Поиск INT 0x29
        if (base[0] == 0xCD && base[1] == 0x29) {
            MemoryUtils::WriteNop(base, 2);
            patchedCount++;
            offset += 2;
            continue;
        }
        
        // Поиск INT1 (0xF1)
        if (base[0] == 0xF1) {
            MemoryUtils::WriteNop(base, 1);
            patchedCount++;
            offset += 1;
            continue;
        }
        
        offset++;
    }
    
    return patchedCount > 0;
}

} // namespace swill
