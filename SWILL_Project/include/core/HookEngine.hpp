#pragma once

#include "shared/SharedDefs.hpp"
#include <vector>

namespace swill {

// ============================================================================
// HookEngine - Реализация кастомного движка хуков
// ============================================================================

inline HookEngine& HookEngine::Instance() {
    static HookEngine instance;
    return instance;
}

inline bool HookEngine::Initialize() {
    if (m_initialized) return true;
    
    // Инициализация успешна
    m_initialized = true;
    return true;
}

inline bool HookEngine::CreateHook(void* targetAddress, void* hookFunction, void** originalFunction) {
    if (!targetAddress || !hookFunction) return false;
    
    // Проверяем, не создан ли уже хук для этого адреса
    for (auto& hook : m_hooks) {
        if (hook.targetAddress == targetAddress) {
            return false; // Хук уже существует
        }
    }
    
    // Выделяем память под трамплин (минимум 16 байт для копирования инструкций)
    size_t trampolineSize = 16;
    void* trampolineBuffer = VirtualAlloc(nullptr, trampolineSize, 
                                          MEM_COMMIT | MEM_RESERVE, 
                                          PAGE_EXECUTE_READWRITE);
    if (!trampolineBuffer) return false;
    
    // Копируем оригинальные инструкции в трамплин
    CopyInstructions(targetAddress, trampolineBuffer, trampolineSize);
    
    // Сохраняем контекст хука
    HookContext ctx;
    ctx.targetAddress = targetAddress;
    ctx.hookFunction = hookFunction;
    ctx.trampolineBuffer = trampolineBuffer;
    ctx.trampolineSize = trampolineSize;
    ctx.isActive = false;
    
    m_hooks.push_back(ctx);
    
    // Возвращаем адрес трамплина как "оригинальную" функцию
    if (originalFunction) {
        *originalFunction = trampolineBuffer;
    }
    
    return true;
}

inline bool HookEngine::EnableHook(void* targetAddress) {
    for (auto& hook : m_hooks) {
        if (hook.targetAddress == targetAddress && !hook.isActive) {
            // Записываем JMP на функцию-хук в начало целевой функции
            WriteJmp(targetAddress, hook.hookFunction);
            hook.isActive = true;
            return true;
        }
    }
    return false;
}

inline bool HookEngine::DisableHook(void* targetAddress) {
    for (auto& hook : m_hooks) {
        if (hook.targetAddress == targetAddress && hook.isActive) {
            // Восстанавливаем оригинальный код из трамплина
            DWORD oldProtect;
            if (VirtualProtect(hook.targetAddress, hook.trampolineSize, 
                              PAGE_EXECUTE_READWRITE, &oldProtect)) {
                memcpy(hook.targetAddress, hook.trampolineBuffer, hook.trampolineSize);
                VirtualProtect(hook.targetAddress, hook.trampolineSize, oldProtect, &oldProtect);
                FlushInstructionCache(GetCurrentProcess(), hook.targetAddress, hook.trampolineSize);
                hook.isActive = false;
                return true;
            }
        }
    }
    return false;
}

inline bool HookEngine::RemoveHook(void* targetAddress) {
    // Сначала отключаем хук
    DisableHook(targetAddress);
    
    // Удаляем из списка и освобождаем память
    for (auto it = m_hooks.begin(); it != m_hooks.end(); ++it) {
        if (it->targetAddress == targetAddress) {
            if (it->trampolineBuffer) {
                VirtualFree(it->trampolineBuffer, 0, MEM_RELEASE);
            }
            m_hooks.erase(it);
            return true;
        }
    }
    return false;
}

inline bool HookEngine::ApplyAllHooks() {
    bool success = true;
    for (auto& hook : m_hooks) {
        if (!hook.isActive) {
            if (!EnableHook(hook.targetAddress)) {
                success = false;
            }
        }
    }
    return success;
}

inline bool HookEngine::RemoveAllHooks() {
    // Создаём копию списка, т.к. RemoveHook модифицирует его
    std::vector<void*> addresses;
    for (const auto& hook : m_hooks) {
        addresses.push_back(hook.targetAddress);
    }
    
    bool success = true;
    for (void* addr : addresses) {
        if (!RemoveHook(addr)) {
            success = false;
        }
    }
    return success;
}

inline HookEngine::~HookEngine() {
    RemoveAllHooks();
}

inline size_t HookEngine::CopyInstructions(void* source, void* destination, size_t minSize) {
    uint8_t* src = static_cast<uint8_t*>(source);
    uint8_t* dst = static_cast<uint8_t*>(destination);
    size_t totalSize = 0;
    
    while (totalSize < minSize) {
        size_t instrSize = GetInstructionSize(src);
        if (instrSize == 0 || instrSize > 15) break; // Защита от мусора
        
        memcpy(dst, src, instrSize);
        
        // Корректировка относительных адресов (JMP/CALL с rel32)
        if (*src == 0xE8 || *src == 0xE9) { // CALL или JMP
            int32_t relOffset = *reinterpret_cast<int32_t*>(src + 1);
            void* targetAddr = src + 5 + relOffset;
            int32_t newRelOffset = reinterpret_cast<int32_t>(
                static_cast<uint8_t*>(targetAddr) - dst - 5);
            *reinterpret_cast<int32_t*>(dst + 1) = newRelOffset;
        }
        
        src += instrSize;
        dst += instrSize;
        totalSize += instrSize;
    }
    
    // Добавляем JMP обратно в оставшуюся часть оригинальной функции
    WriteJmp(dst, source + totalSize);
    totalSize += 5;
    
    return totalSize;
}

inline size_t HookEngine::GetInstructionSize(const uint8_t* code) {
    // Упрощённый декодер инструкций x86
    // Для продакшена нужен полноценный дизассемблер (например, Zydis)
    
    if (!code) return 0;
    
    uint8_t opcode = code[0];
    
    // Префиксы (игнорируем)
    while (opcode == 0x66 || opcode == 0x67 || opcode == 0xF0 || 
           opcode == 0xF2 || opcode == 0xF3 || 
           (opcode >= 0x40 && opcode <= 0x4F)) {
        code++;
        opcode = code[0];
    }
    
    // Основные опкоды
    switch (opcode) {
        case 0x00 ... 0x05: return 1; // ADD al/mem, imm
        case 0x06 ... 0x07: return 1; // PUSH/POP ES/DS
        case 0x08 ... 0x0D: return 1; // OR
        case 0x0E: return 1;          // PUSH CS
        case 0x0F: {                  // Двухбайтовый опкод
            uint8_t op2 = code[1];
            if (op2 >= 0x80 && op2 <= 0x8F) return 6; // Jcc rel32
            if (op2 >= 0x40 && op2 <= 0x4F) return 3; // CMOVcc
            return 2; // Стандартный двухбайтовый
        }
        case 0x10 ... 0x1D: return 1; // ADC
        case 0x1E: return 1;          // PUSH DS
        case 0x1F: return 1;          // POP DS
        case 0x20 ... 0x2D: return 1; // AND
        case 0x2E: return 2;          // Prefix CS
        case 0x30 ... 0x3D: return 1; // XOR
        case 0x3E: return 2;          // Prefix DS
        case 0x40 ... 0x4F: return 1; // INC/DEC reg
        case 0x50 ... 0x57: return 1; // PUSH reg
        case 0x58 ... 0x5F: return 1; // POP reg
        case 0x60: return 1;          // PUSHAD
        case 0x61: return 1;          // POPAD
        case 0x62: return 1;          // BOUND
        case 0x63: return 1;          // ARPL/MOVSXD
        case 0x64: return 2;          // Prefix FS
        case 0x65: return 2;          // Prefix GS
        case 0x66: return 2;          // Prefix operand-size
        case 0x67: return 2;          // Prefix address-size
        case 0x68: return 5;          // PUSH imm32
        case 0x69: return 6;          // IMUL r32, r/m32, imm32
        case 0x6A: return 2;          // PUSH imm8
        case 0x6B: return 3;          // IMUL r32, r/m32, imm8
        case 0x6C: return 1;          // INS
        case 0x6D: return 1;          // INSD
        case 0x6E: return 1;          // OUTS
        case 0x6F: return 1;          // OUTSD
        case 0x70 ... 0x7F: return 2; // Jcc rel8
        case 0x80: return 3;          // GRP1 Eb, Ib
        case 0x81: return 6;          // GRP1 Ev, Iz
        case 0x82: return 3;          // GRP1 Eb, Ib (deprecated)
        case 0x83: return 3;          // GRP1 Ev, Ib
        case 0x84: return 2;          // TEST
        case 0x85: return 2;          // TEST
        case 0x86: return 2;          // XCHG
        case 0x87: return 2;          // XCHG
        case 0x88: return 2;          // MOV
        case 0x89: return 2;          // MOV
        case 0x8A: return 2;          // MOV
        case 0x8B: return 2;          // MOV
        case 0x8C: return 2;          // MOV
        case 0x8D: return 2;          // LEA
        case 0x8E: return 2;          // MOV
        case 0x8F: return 2;          // POP
        case 0x90: return 1;          // NOP/XCHG
        case 0x91 ... 0x97: return 1; // XCHG
        case 0x98: return 1;          // CBW
        case 0x99: return 1;          // CWD
        case 0x9A: return 5;          // CALL ptr16:32
        case 0x9B: return 1;          // WAIT
        case 0x9C: return 1;          // PUSHFD
        case 0x9D: return 1;          // POPFD
        case 0x9E: return 1;          // SAHF
        case 0x9F: return 1;          // LAHF
        case 0xA0: return 5;          // MOV al, moffs8
        case 0xA1: return 5;          // MOV eAX, moffs32
        case 0xA2: return 5;          // MOV moffs8, al
        case 0xA3: return 5;          // MOV moffs32, eAX
        case 0xA4: return 1;          // MOVS
        case 0xA5: return 1;          // MOVSD
        case 0xA6: return 1;          // CMPS
        case 0xA7: return 1;          // CMPSD
        case 0xA8: return 2;          // TEST AL, imm8
        case 0xA9: return 5;          // TEST EAX, imm32
        case 0xAA: return 1;          // STOS
        case 0xAB: return 1;          // STOSD
        case 0xAC: return 1;          // LODS
        case 0xAD: return 1;          // LODSD
        case 0xAE: return 1;          // SCAS
        case 0xAF: return 1;          // SCASD
        case 0xB0 ... 0xB7: return 2; // MOV reg8, imm8
        case 0xB8 ... 0xBF: return 5; // MOV reg32, imm32
        case 0xC0: return 3;          // GRP2 Eb, Ib
        case 0xC1: return 3;          // GRP2 Ev, Ib
        case 0xC2: return 3;          // RETN imm16
        case 0xC3: return 1;          // RETN
        case 0xC4: return 2;          // LES
        case 0xC5: return 2;          // LDS
        case 0xC6: return 3;          // GRP11 Eb, Ib
        case 0xC7: return 6;          // GRP11 Ev, Iz
        case 0xC8: return 4;          // ENTER
        case 0xC9: return 1;          // LEAVE
        case 0xCA: return 3;          // RETF
        case 0xCB: return 1;          // RETF
        case 0xCC: return 1;          // INT3
        case 0xCD: return 2;          // INT imm8
        case 0xCE: return 1;          // INTO
        case 0xCF: return 1;          // IRET
        case 0xD0: return 2;          // GRP2 Eb, 1
        case 0xD1: return 2;          // GRP2 Ev, 1
        case 0xD2: return 2;          // GRP2 Eb, CL
        case 0xD3: return 2;          // GRP2 Ev, CL
        case 0xD4: return 2;          // AAM
        case 0xD5: return 2;          // AAD
        case 0xD6: return 1;          // SALC
        case 0xD7: return 1;          // XLAT
        case 0xD8 ... 0xDF: return 2; // FPU
        case 0xE0: return 2;          // LOOPNE
        case 0xE1: return 2;          // LOOPE
        case 0xE2: return 2;          // LOOP
        case 0xE3: return 2;          // JCXZ
        case 0xE4: return 2;          // IN AL, imm8
        case 0xE5: return 2;          // IN eAX, imm8
        case 0xE6: return 2;          // OUT imm8, AL
        case 0xE7: return 2;          // OUT imm8, eAX
        case 0xE8: return 5;          // CALL rel32
        case 0xE9: return 5;          // JMP rel32
        case 0xEA: return 7;          // JMP ptr16:32
        case 0xEB: return 2;          // JMP rel8
        case 0xEC: return 1;          // IN AL, DX
        case 0xED: return 1;          // IN eAX, DX
        case 0xEE: return 1;          // OUT DX, AL
        case 0xEF: return 1;          // OUT DX, eAX
        case 0xF0: return 2;          // LOCK prefix
        case 0xF1: return 1;          // INT1
        case 0xF2: return 2;          // REPNE prefix
        case 0xF3: return 2;          // REPE prefix
        case 0xF4: return 1;          // HLT
        case 0xF5: return 1;          // CMC
        case 0xF6: return 3;          // GRP3a Eb, Ib
        case 0xF7: return 6;          // GRP3b Ev, Iz
        case 0xF8: return 1;          // CLC
        case 0xF9: return 1;          // STC
        case 0xFA: return 1;          // CLI
        case 0xFB: return 1;          // STI
        case 0xFC: return 1;          // CLD
        case 0xFD: return 1;          // STD
        case 0xFE: return 2;          // GRP4 Eb
        case 0xFF: {                  // GRP5 Ev
            uint8_t modrm = code[1];
            uint8_t reg = (modrm >> 3) & 0x07;
            if (reg == 2 || reg == 4) return 2; // CALL/JMP near
            return 2;
        }
        default: return 1; // По умолчанию 1 байт
    }
}

inline void HookEngine::WriteJmp(void* source, void* destination) {
    DWORD oldProtect;
    if (!VirtualProtect(source, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return;
    }
    
    uint8_t* src = static_cast<uint8_t*>(source);
    
    // Записываем JMP rel32 (E9 + смещение)
    src[0] = 0xE9;
    int32_t relOffset = reinterpret_cast<int32_t>(
        static_cast<uint8_t*>(destination) - src - 5);
    *reinterpret_cast<int32_t*>(src + 1) = relOffset;
    
    VirtualProtect(source, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), source, 5);
}

inline bool HookEngine::ProtectMemory(void* address, size_t size, 
                                       DWORD newProtect, PDWORD oldProtect) {
    return VirtualProtect(address, size, newProtect, oldProtect) != FALSE;
}

} // namespace swill
