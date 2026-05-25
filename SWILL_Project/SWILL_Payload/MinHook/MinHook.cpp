/*
 * MinHook - The Minimalistic API Hooking Library for x64/x86
 * Copyright (C) 2009-2019 Tsuda Kageyu.
 * All rights reserved.
 */

#include "MinHook.h"
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <cstring>

#pragma comment(lib, "psapi.lib")

// Внутренние структуры MinHook
struct HOOK_ENTRY {
    BYTE    backup[8];
    BYTE    patch[14];
    LPVOID  pTarget;
    LPVOID  pDetour;
    LPVOID  pTrampoline;
    BOOL    enabled;
};

static std::vector<HOOK_ENTRY> g_hooks;
static BOOL g_initialized = FALSE;

// Код трамплина для x86
static LPVOID CreateTrampoline(LPVOID pTarget, size_t size) {
    if (!pTarget || size == 0) return nullptr;
    
    LPVOID pTrampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pTrampoline) return nullptr;
    
    memcpy(pTrampoline, pTarget, size);
    
    BYTE* pJump = (BYTE*)((BYTE*)pTrampoline + size);
    pJump[0] = 0xE9;
    *(DWORD*)(pJump + 1) = (DWORD)((BYTE*)pTarget + size - (BYTE*)pJump - 5);
    
    return pTrampoline;
}

extern "C" {

MH_STATUS WINAPI MH_Initialize(void) {
    if (g_initialized) return MH_ERROR_ALREADY_INITIALIZED;
    
    g_hooks.clear();
    g_initialized = TRUE;
    return MH_OK;
}

MH_STATUS WINAPI MH_Uninitialize(void) {
    if (!g_initialized) return MH_ERROR_NOT_INITIALIZED;
    
    for (auto& hook : g_hooks) {
        if (hook.enabled) {
            MH_DisableHook(hook.pTarget);
        }
        if (hook.pTrampoline) {
            VirtualFree(hook.pTrampoline, 0, MEM_RELEASE);
        }
    }
    
    g_hooks.clear();
    g_initialized = FALSE;
    return MH_OK;
}

MH_STATUS WINAPI MH_CreateHook(LPVOID pTarget, LPVOID pDetour, LPVOID *ppOriginal) {
    if (!g_initialized) return MH_ERROR_NOT_INITIALIZED;
    if (!pTarget || !pDetour) return MH_ERROR_INVALID_CALL;
    
    HOOK_ENTRY hook = {0};
    hook.pTarget = pTarget;
    hook.pDetour = pDetour;
    
    size_t hookSize = 6;
    hook.pTrampoline = CreateTrampoline(pTarget, hookSize);
    if (!hook.pTrampoline) return MH_ERROR_MEMORY_ALLOC;
    
    memcpy(hook.backup, pTarget, hookSize);
    
    if (ppOriginal) {
        *ppOriginal = hook.pTrampoline;
    }
    
    g_hooks.push_back(hook);
    return MH_OK;
}

MH_STATUS WINAPI MH_RemoveHook(LPVOID pTarget) {
    if (!g_initialized) return MH_ERROR_NOT_INITIALIZED;
    
    for (auto it = g_hooks.begin(); it != g_hooks.end(); ++it) {
        if (it->pTarget == pTarget) {
            if (it->enabled) {
                MH_DisableHook(pTarget);
            }
            if (it->pTrampoline) {
                VirtualFree(it->pTrampoline, 0, MEM_RELEASE);
            }
            g_hooks.erase(it);
            return MH_OK;
        }
    }
    
    return MH_ERROR_NOT_CREATED;
}

MH_STATUS WINAPI MH_EnableHook(LPVOID pTarget) {
    if (!g_initialized) return MH_ERROR_NOT_INITIALIZED;
    
    for (auto& hook : g_hooks) {
        if (hook.pTarget == pTarget || pTarget == MH_ALL_HOOKS) {
            if (hook.enabled) continue;
            
            DWORD oldProtect;
            if (!VirtualProtect(hook.pTarget, 14, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                return MH_ERROR_MEMORY_PROTECT;
            }
            
            BYTE* pByte = (BYTE*)hook.pTarget;
            pByte[0] = 0xE9;
            *(DWORD*)(pByte + 1) = (DWORD)((BYTE*)hook.pDetour - pByte - 5);
            
            VirtualProtect(hook.pTarget, 14, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), hook.pTarget, 14);
            
            hook.enabled = TRUE;
        }
    }
    
    return MH_OK;
}

MH_STATUS WINAPI MH_DisableHook(LPVOID pTarget) {
    if (!g_initialized) return MH_ERROR_NOT_INITIALIZED;
    
    for (auto& hook : g_hooks) {
        if (hook.pTarget == pTarget || pTarget == MH_ALL_HOOKS) {
            if (!hook.enabled) continue;
            
            DWORD oldProtect;
            if (!VirtualProtect(hook.pTarget, 8, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                return MH_ERROR_MEMORY_PROTECT;
            }
            
            memcpy(hook.pTarget, hook.backup, 8);
            
            VirtualProtect(hook.pTarget, 8, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), hook.pTarget, 8);
            
            hook.enabled = FALSE;
        }
    }
    
    return MH_OK;
}

MH_STATUS WINAPI MH_QueueEnableHook(LPVOID pTarget) {
    return MH_EnableHook(pTarget);
}

MH_STATUS WINAPI MH_QueueDisableHook(LPVOID pTarget) {
    return MH_DisableHook(pTarget);
}

MH_STATUS WINAPI MH_ApplyQueuedHooks(void) {
    return MH_OK;
}

MH_STATUS WINAPI MH_CreateHookApi(LPCWSTR pszModule, LPCSTR pszProcName, LPVOID pDetour, LPVOID *ppOriginal) {
    HMODULE hModule = GetModuleHandleW(pszModule);
    if (!hModule) return MH_ERROR_INVALID_CALL;
    
    LPVOID pTarget = (LPVOID)GetProcAddress(hModule, pszProcName);
    if (!pTarget) return MH_ERROR_INVALID_CALL;
    
    return MH_CreateHook(pTarget, pDetour, ppOriginal);
}

MH_STATUS WINAPI MH_CreateHookApiEx(LPCWSTR pszModule, LPCSTR pszProcName, LPVOID pDetour, LPVOID *ppOriginal, LPVOID *ppTarget) {
    MH_STATUS status = MH_CreateHookApi(pszModule, pszProcName, pDetour, ppOriginal);
    if (status == MH_OK && ppTarget) {
        HMODULE hModule = GetModuleHandleW(pszModule);
        *ppTarget = (LPVOID)GetProcAddress(hModule, pszProcName);
    }
    return status;
}

}
