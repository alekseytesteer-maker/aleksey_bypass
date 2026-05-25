/*
 * MinHook - The Minimalistic API Hooking Library for x64/x86
 * Copyright (C) 2009-2019 Tsuda Kageyu.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if defined(_M_IX86) || defined(__i386__)
#define MH_X86
#elif defined(_M_X64) || defined(__x86_64__)
#define MH_X64
#else
#error "Unsupported architecture. Only x86 and x64 are supported."
#endif

#include <windows.h>

typedef enum MH_STATUS {
    MH_UNKNOWN = -1,
    MH_OK = 0,
    MH_ERROR_ALREADY_INITIALIZED = 1,
    MH_ERROR_NOT_INITIALIZED = 2,
    MH_ERROR_ALREADY_CREATED = 3,
    MH_ERROR_NOT_CREATED = 4,
    MH_ERROR_ENABLED = 5,
    MH_ERROR_DISABLED = 6,
    MH_ERROR_TOO_MANY_HOOKS = 7,
    MH_ERROR_INVALID_CALL = 8,
    MH_ERROR_MEMORY_ALLOC = 9,
    MH_ERROR_MEMORY_PROTECT = 10,
    MH_ERROR_UNSUPPORTED_FUNCTION = 11,
    MH_ERROR_BAD_POINTER = 12,
    MH_ERROR_INVALID_PARAMETER = 13
} MH_STATUS;

typedef enum MH_HOOK_FLAG {
    MH_HOOK_NONE = 0,
    MH_HOOK_IAT = 1,
    MH_HOOK_INLINE = 2
} MH_HOOK_FLAG;

#ifdef __cplusplus
extern "C" {
#endif

    MH_STATUS WINAPI MH_Initialize(void);
    MH_STATUS WINAPI MH_Uninitialize(void);
    MH_STATUS WINAPI MH_CreateHook(LPVOID pTarget, LPVOID pDetour, LPVOID *ppOriginal);
    MH_STATUS WINAPI MH_CreateHookApiEx(LPCWSTR pszModule, LPCSTR pszProcName, LPVOID pDetour, LPVOID *ppOriginal, LPVOID *ppTarget);
    MH_STATUS WINAPI MH_RemoveHook(LPVOID pTarget);
    MH_STATUS WINAPI MH_EnableHook(LPVOID pTarget);
    MH_STATUS WINAPI MH_DisableHook(LPVOID pTarget);
    MH_STATUS WINAPI MH_QueueEnableHook(LPVOID pTarget);
    MH_STATUS WINAPI MH_QueueDisableHook(LPVOID pTarget);
    MH_STATUS WINAPI MH_ApplyQueuedHooks(void);
    MH_STATUS WINAPI MH_CreateHookApi(LPCWSTR pszModule, LPCSTR pszProcName, LPVOID pDetour, LPVOID *ppOriginal);

#ifdef __cplusplus
}
#endif
