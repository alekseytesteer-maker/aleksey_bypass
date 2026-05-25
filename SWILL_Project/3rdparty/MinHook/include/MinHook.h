#pragma once
#include <windows.h>

typedef enum MH_STATUS {
    MH_UNKNOWN = 0,
    MH_OK = 1,
    MH_ERROR_ALREADY_INITIALIZED = 2,
    MH_ERROR_NOT_INITIALIZED = 3,
    MH_ERROR_ALREADY_CREATED = 4,
    MH_ERROR_NOT_CREATED = 5,
    MH_ERROR_ENABLED = 6,
    MH_ERROR_DISABLED = 7,
    MH_ERROR_MULTIPLE_THREADS = 8,
    MH_ERROR_UNSUPPORTED_FUNCTION = 9,
    MH_ERROR_MEMORY_ALLOC = 10,
    MH_ERROR_MEMORY_PROTECT = 11,
    MH_ERROR_MODULE_NOT_FOUND = 12,
    MH_ERROR_INVALID_SCAN_PATTERN = 13
} MH_STATUS;

#ifdef __cplusplus
extern "C" {
#endif

    MH_STATUS MH_Initialize(void);
    MH_STATUS MH_Uninitialize(void);
    MH_STATUS MH_CreateHook(LPVOID pTarget, LPVOID pDetour, LPVOID *ppOriginal);
    MH_STATUS MH_RemoveHook(LPVOID pTarget);
    MH_STATUS MH_EnableHook(LPVOID pTarget);
    MH_STATUS MH_DisableHook(LPVOID pTarget);
    MH_STATUS MH_ApplyQueued(void);

#ifdef __cplusplus
}
#endif
