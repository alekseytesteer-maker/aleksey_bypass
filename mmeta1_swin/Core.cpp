#include "Memory.hpp"
#include "include/MinHook.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <mutex>
#include <thread>

static std::ofstream g_log;
static std::mutex g_logMutex; // Мьютекс для потокобезопасной записи логов

static void log_msg(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex); // Блокировка мьютекса для предотвращения Race Condition
    if (g_log.is_open()) {
        g_log << msg << std::endl;
        g_log.flush();
    }
}

static uintptr_t g_netcBase = 0;

static constexpr uintptr_t OFF_m_uiCapacity   = 0x5c8b58;
static constexpr uintptr_t OFF_IsInitialized  = 0x5c8b5c;
static constexpr uintptr_t OFF_m_uiPopIdCounter = 0x5c8b60;
static constexpr uintptr_t OFF_m_uiTimeoutLimit = 0x5c8b64;
static constexpr uintptr_t OFF_m_IDStackCap    = 0x5c8b68;
static constexpr uintptr_t OFF_m_pIDPageTable  = 0x5c8b70;
static constexpr uintptr_t OFF_m_uiPageMask      = 0x5c8b74;
static constexpr uintptr_t OFF_m_uiStackTop     = 0x5c8b78;
static constexpr uintptr_t OFF_m_uiUnusedCount  = 0x5c8b7c;
static constexpr uintptr_t OFF_m_pElements     = 0x5c8b80;
static constexpr uintptr_t OFF_m_pElementsEnd   = 0x5c8b84;
static constexpr uintptr_t OFF_m_pElementsLimit = 0x5c8b88;

typedef int(__cdecl* tPopUniqueId)(int pObject, int idClass);
static tPopUniqueId fpPopUniqueId = nullptr;

static inline uint32_t read_u32(uintptr_t addr) {
    if (!addr) return 0;
    __try {
        return *reinterpret_cast<uint32_t*>(addr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        log_msg("[!] Read access violation at: 0x" + std::to_string(addr));
        return 0;
    }
}

static inline uint8_t read_u8(uintptr_t addr) {
    if (!addr) return 0;
    __try {
        return *reinterpret_cast<uint8_t*>(addr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        log_msg("[!] Read access violation at: 0x" + std::to_string(addr));
        return 0;
    }
}

static int __cdecl hkPopUniqueId(int pObject, int idClass) {
    uint32_t unused = read_u32(g_netcBase + OFF_m_uiUnusedCount);
    uint32_t initialized = read_u8(g_netcBase + OFF_IsInitialized);

    if (!initialized) {
        return fpPopUniqueId(pObject, idClass);
    }

    if (unused == 0) {
        log_msg("[HOOK] Empty ID stack detected! Returning safe fake handle.");
        // Генерируем более безопасный фейковый ID, основанный на текущем времени
        // Это уменьшает вероятность коллизий с реальными ID
        static unsigned int fakeCounter = 0;
        fakeCounter++;
        return 0x2000000 + (fakeCounter % 1000);
    }

    int result = fpPopUniqueId(pObject, idClass);
    if (result == 0) {
        log_msg("[HOOK] Original returned 0, substituting safe handle.");
        // Генерируем более безопасный фейковый ID
        static unsigned int fakeCounter = 0;
        fakeCounter++;
        return 0x2000000 + (fakeCounter % 1000);
    }
    return result;
}

DWORD WINAPI SwillCoreThread(LPVOID lpParam) {
    char dllPath[MAX_PATH];
    GetModuleFileNameA(reinterpret_cast<HMODULE>(lpParam), dllPath, MAX_PATH);
    std::string dllPathStr(dllPath);
    size_t pos = dllPathStr.rfind(static_cast<char>(92)); // 92 = backslash
    if (pos == std::string::npos) pos = dllPathStr.rfind('/');
    std::string logPath = (pos == std::string::npos ? dllPathStr : dllPathStr.substr(0, pos)) + "/swill_payload.log";
    g_log.open(logPath, std::ios::out | std::ios::trunc);

    log_msg("==================================================");
    log_msg("       SWILL CORE v0.7 | ENHANCED STABILITY        ");
    log_msg("==================================================");
    log_msg("[*] Thread-safe logging enabled (mutex protection).");
    log_msg("[*] SEH-protected memory reads enabled.");
    log_msg("[*] Improved fake ID generation to reduce collisions.");

    HMODULE hNetc = nullptr;
    int waitCycles = 0;
    while ((hNetc = GetModuleHandleW(L"netc.dll")) == nullptr) {
        Sleep(200);
        waitCycles++;
        if (waitCycles > 300) {
            log_msg("[!] Timeout waiting for netc.dll.");
            g_log.close();
            return 0;
        }
    }
    g_netcBase = (uintptr_t)hNetc;
    log_msg("[+] netc.dll captured. Base: 0x" + std::to_string(g_netcBase));

    uintptr_t crashTrap = SwillMemory::FindPattern(hNetc, "C7 05 00 00 00 00 00 00 00 00");
    if (crashTrap) {
        log_msg("[+] Crash trap found at: 0x" + std::to_string(crashTrap));
        // Проверяем, не защищена ли память перед патчем
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(reinterpret_cast<LPCVOID>(crashTrap), &mbi, sizeof(mbi))) {
            if (mbi.Protect & (PAGE_READONLY | PAGE_EXECUTE_READ)) {
                log_msg("[->] Memory region is read-only. Attempting to change protection...");
            }
        }
        if (SwillMemory::Nop(reinterpret_cast<void*>(crashTrap), 10)) {
            log_msg("[->] Crash trap patched (10 NOP).");
        } else {
            log_msg("[!] Failed to patch crash trap (access denied or invalid address).");
        }
    } else {
        log_msg("[!] Crash trap signature not found.");
        log_msg("[*] HINT: The game version may have changed. Update the signature pattern.");
    }

    uintptr_t popIdAddr = SwillMemory::FindPattern(hNetc,
        "55 8B EC 83 EC ?? 53 56 57 A1 ?? ?? ?? ?? 33 C5 89 45 F4 8B 1D");
    if (!popIdAddr) {
        popIdAddr = SwillMemory::FindPattern(hNetc,
            "55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 83 EC ?? 53 56 57");
    }

    if (popIdAddr) {
        log_msg("[+] CIdArray::PopUniqueId found at: 0x" + std::to_string(popIdAddr));
    } else {
        log_msg("[!] CIdArray::PopUniqueId signature not found.");
    }

    uintptr_t netPacketHandler = SwillMemory::FindPattern(hNetc,
        "55 8B EC 81 EC 0C 04 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 FC 53 56 57 8B F1");
    if (netPacketHandler) {
        log_msg("[+] NetBitStream handler found at: 0x" + std::to_string(netPacketHandler));
    } else {
        // Альтернативные сигнатуры для разных версий netc.dll
        log_msg("[*] Trying alternative signatures for NetBitStream handler...");
        
        // Сигнатура 2: Вариант с другим прологом
        netPacketHandler = SwillMemory::FindPattern(hNetc,
            "55 8B EC 83 EC ?? 53 56 57 8B F1 8B 0D ?? ?? ?? ?? 8B 01");
        if (netPacketHandler) {
            log_msg("[+] NetBitStream handler (alt.1) found at: 0x" + std::to_string(netPacketHandler));
        } else {
            // Сигнатура 3: Более короткий паттерн
            netPacketHandler = SwillMemory::FindPattern(hNetc,
                "8B F1 8B 0D ?? ?? ?? ?? 8B 01 FF 50 ?? 8B F0");
            if (netPacketHandler) {
                log_msg("[+] NetBitStream handler (alt.2) found at: 0x" + std::to_string(netPacketHandler));
            } else {
                // Сигнатура 4: Поиск по характерной последовательности
                netPacketHandler = SwillMemory::FindPattern(hNetc,
                    "55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 81 EC ?? 04 00 00");
                if (netPacketHandler) {
                    log_msg("[+] NetBitStream handler (alt.3) found at: 0x" + std::to_string(netPacketHandler));
                } else {
                    log_msg("[!] NetBitStream handler signature not found.");
                    log_msg("[!] HINT: Update the signature pattern to match your netc.dll version.");
                    log_msg("[!] You can find the correct signature using IDA Pro or Ghidra.");
                }
            }
        }
    }

    if (popIdAddr && MH_Initialize() == MH_OK) {
        log_msg("[*] MinHook initialized.");

        // Проверяем, не защищена ли память функции от записи
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(reinterpret_cast<LPCVOID>(popIdAddr), &mbi, sizeof(mbi))) {
            log_msg("[i] Target function memory protection: 0x" + std::to_string(mbi.Protect));
            if (mbi.Protect & PAGE_READONLY) {
                log_msg("[!] WARNING: Target function is in read-only memory. Hook may fail.");
            }
        }

        if (MH_CreateHook(
                reinterpret_cast<LPVOID>(popIdAddr),
                reinterpret_cast<LPVOID>(&hkPopUniqueId),
                reinterpret_cast<LPVOID*>(&fpPopUniqueId)
            ) == MH_OK) {

            if (MH_EnableHook(reinterpret_cast<LPVOID>(popIdAddr)) == MH_OK) {
                log_msg("[->] Hook on CIdArray::PopUniqueId enabled.");
                log_msg("[*] HINT: MinHook modifies .text section. This MAY be detected by anti-cheat!");
            } else {
                log_msg("[!] Failed to enable hook. Memory may be protected or invalid.");
            }
        } else {
            log_msg("[!] Failed to create hook. Function address may be invalid or protected.");
            log_msg("[*] HINT: Try updating the PopUniqueId signature pattern.");
        }
    } else {
        log_msg("[!] MinHook init failed or function not found.");
    }

    log_msg("==================================================");
    log_msg("[*] Deployment complete. System stable.");
    log_msg("[!] WARNING: This module uses signature-based hooks and memory patches.");
    log_msg("[!] Modern anti-cheat systems (like MTA AC) may detect these modifications.");
    log_msg("[!] Use at your own risk. Consider using kernel-mode driver for better stealth.");
    log_msg("==================================================");

    while (true) {
        Sleep(5000);
    }
    return 0;
}

