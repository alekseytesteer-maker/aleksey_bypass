// SWILL Payload - Core Logic with CIdArray Hook
#include "Memory.hpp"
#include "Hooks.hpp"
#include <windows.h>
#include <iostream>

#pragma comment(lib, "psapi.lib")

// Global variables
TPopUniqueId g_OriginalPopUniqueId = nullptr;
uintptr_t g_CIdArray_Base = 0;
uintptr_t g_CIdArray_IsInitialized = 0;
uintptr_t g_CIdArray_IDStackCount = 0;

// Hooked function for CIdArray::PopUniqueId (FUN_10241990)
unsigned int __stdcall Hooked_PopUniqueId(void* param_1, void* param_2) {
    // Check if initialized
    char isInit = *(char*)(g_CIdArray_IsInitialized);
    if (!isInit) {
        return g_OriginalPopUniqueId(param_1, param_2);
    }

    // Check stack count
    int stackCount = *(int*)(g_CIdArray_IDStackCount);
    
    if (stackCount <= 0) {
        std::cout << "[HOOK] Prevented crash! Empty ID stack. Generating virtual ID..." << std::endl;
        
        // Generate safe virtual ID
        unsigned int capacity = *(unsigned int*)(g_CIdArray_Base + 0x04);
        unsigned int fakeIndex = capacity + 1000;
        
        return fakeIndex + 0x2000000;
    }

    // Call original function
    return g_OriginalPopUniqueId(param_1, param_2);
}

DWORD WINAPI SwillCoreThread(LPVOID lpParam) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    std::cout << "==================================================" << std::endl;
    std::cout << "       SWILL CORE v1.0 | MINHOOK INTEGRATION      " << std::endl;
    std::cout << "==================================================" << std::endl;

    // Initialize MinHook
    if (!SwillHooks::Initialize()) {
        fclose(stdout);
        FreeConsole();
        return 1;
    }

    // Wait for netc.dll
    HMODULE hNetc = nullptr;
    std::cout << "[*] Waiting for netc.dll..." << std::endl;
    while ((hNetc = GetModuleHandleW(L"netc.dll")) == nullptr) {
        Sleep(200);
    }
    std::cout << "[+] netc.dll loaded at: 0x" << std::hex << (uintptr_t)hNetc << std::dec << std::endl;

    // Find CIdArray::PopUniqueId function
    std::cout << "[*] Scanning for PopUniqueId signature..." << std::endl;
    uintptr_t funcAddr = SwillMemory::FindPattern(hNetc, 
        "55 8B EC 83 EC ?? 53 56 57 A1 ?? ?? ?? ?? 33 C5 89 45 F4 8B 1D");
    
    if (!funcAddr) {
        std::cout << "[!] CRITICAL: PopUniqueId function not found!" << std::endl;
        fclose(stdout);
        FreeConsole();
        return 1;
    }

    std::cout << "[+] Found PopUniqueId at: 0x" << std::hex << funcAddr << std::dec << std::endl;

    // Extract CIdArray base address from function
    uintptr_t globalPtrAddr = *(uintptr_t*)(funcAddr + 11);
    g_CIdArray_Base = globalPtrAddr;
    
    // Calculate field offsets based on decompiled code analysis
    g_CIdArray_IsInitialized = g_CIdArray_Base + 0x04;  // DAT_105c8b5c
    g_CIdArray_IDStackCount  = g_CIdArray_Base + 0x24;  // DAT_105c8b7c

    std::cout << "[->] CIdArray Base: 0x" << std::hex << g_CIdArray_Base << std::dec << std::endl;
    std::cout << "[->] IsInitialized: 0x" << std::hex << g_CIdArray_IsInitialized << std::dec << std::endl;
    std::cout << "[->] IDStackCount: 0x" << std::hex << g_CIdArray_IDStackCount << std::dec << std::endl;

    // Install hook
    std::cout << "[*] Installing CIdArray::PopUniqueId hook..." << std::endl;
    
    if (SwillHooks::CreateHook((void*)funcAddr, (void*)Hooked_PopUniqueId, 
                               (void**)&g_OriginalPopUniqueId, "CIdArray::PopUniqueId")) {
        std::cout << "[+++++] Hook installed successfully! Crash protection active." << std::endl;
    } else {
        std::cout << "[!] Failed to install hook" << std::endl;
    }

    // Find NetBitStream packet handler
    std::cout << "[*] Scanning for NetBitStream handler..." << std::endl;
    uintptr_t netPacketHandler = SwillMemory::FindPattern(hNetc, 
        "55 8B EC 81 EC 0C 04 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 FC 53 56 57 8B F1");
    
    if (netPacketHandler) {
        std::cout << "[+] Found NetBitStream handler at: 0x" << std::hex << netPacketHandler << std::dec << std::endl;
        std::cout << "[*] Ready for packet interception hook (next phase)" << std::endl;
    } else {
        std::cout << "[!] NetBitStream handler not found" << std::endl;
    }

    // Neutralize crash trap (mov [0], 0)
    std::cout << "[*] Scanning for crash trap..." << std::endl;
    uintptr_t crashTrap = SwillMemory::FindPattern(hNetc, "C7 05 00 00 00 00 00 00 00 00");
    if (crashTrap) {
        std::cout << "[+] Found crash trap at: 0x" << std::hex << crashTrap << std::dec << std::endl;
        if (SwillMemory::Nop((void*)crashTrap, 10)) {
            std::cout << "[->] Crash trap neutralized (NOP'd)" << std::endl;
        }
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "[*] SWILL Core deployment complete. System stable." << std::endl;
    std::cout << "==================================================" << std::endl;

    // Keep thread alive
    while (true) {
        Sleep(1000);
    }
}
