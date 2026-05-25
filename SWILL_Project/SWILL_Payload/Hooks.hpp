// SWILL Payload - MinHook Wrapper
#pragma once

#include "MinHook/MinHook.h"
#include <iostream>
#include <string>

class SwillHooks {
private:
    static bool initialized;

public:
    static bool Initialize() {
        if (initialized) return true;
        
        MH_STATUS status = MH_Initialize();
        if (status != MH_OK) {
            std::cout << "[!] MinHook initialization failed" << std::endl;
            return false;
        }
        
        initialized = true;
        std::cout << "[+] MinHook initialized successfully" << std::endl;
        return true;
    }

    static bool CreateHook(void* target, void* detour, void** original, const char* name) {
        if (!initialized) {
            if (!Initialize()) return false;
        }

        MH_STATUS status = MH_CreateHook(target, detour, original);
        if (status != MH_OK) {
            std::cout << "[!] Failed to create hook: " << name << std::endl;
            return false;
        }

        status = MH_EnableHook(target);
        if (status != MH_OK) {
            std::cout << "[!] Failed to enable hook: " << name << std::endl;
            return false;
        }

        std::cout << "[+] Hook installed: " << name << std::endl;
        return true;
    }

    static void Shutdown() {
        if (!initialized) return;
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        initialized = false;
    }
};

bool SwillHooks::initialized = false;

typedef unsigned int (__stdcall *TPopUniqueId)(void*, void*);

