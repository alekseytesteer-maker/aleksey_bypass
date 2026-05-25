// SWILL Project - Advanced MTA Injector & Payload
// Author: SWILL Team
// Version: 1.0

#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <aclapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")

// Forward declarations
extern unsigned char g_payload[];
extern unsigned int g_payload_len;

// ============================================================================
// Logger System - Advanced logging with timestamps and file output
// ============================================================================
class SwillLogger {
private:
    static std::string logFilePath;
    static HANDLE hLogFile;
    static CRITICAL_SECTION logLock;
    static bool initialized;

    static void Initialize() {
        if (initialized) return;
        
        InitializeCriticalSection(&logLock);
        
        char path[MAX_PATH];
        GetEnvironmentVariableA("USERPROFILE", path, MAX_PATH);
        logFilePath = std::string(path) + "\\Desktop\\swill_injector.log";
        
        hLogFile = CreateFileA(logFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 
                               NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        
        initialized = true;
        Log("[SWILL] Logger initialized");
    }

public:
    static void Log(const char* msg) {
        if (!initialized) Initialize();
        
        EnterCriticalSection(&logLock);
        
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeBuf[64];
        sprintf_s(timeBuf, sizeof(timeBuf), 
                  "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
                  st.wYear, st.wMonth, st.wDay,
                  st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        
        std::cout << timeBuf << msg << std::endl;
        
        if (hLogFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            SetFilePointer(hLogFile, 0, NULL, FILE_END);
            WriteFile(hLogFile, timeBuf, (DWORD)strlen(timeBuf), &written, NULL);
            WriteFile(hLogFile, msg, (DWORD)strlen(msg), &written, NULL);
            WriteFile(hLogFile, "\r\n", 2, &written, NULL);
            FlushFileBuffers(hLogFile);
        }
        
        LeaveCriticalSection(&logLock);
    }

    static void LogF(const char* fmt, ...) {
        char msg[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);
        Log(msg);
    }

    static void Shutdown() {
        if (!initialized) return;
        Log("[SWILL] Logger shutting down");
        if (hLogFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hLogFile);
            hLogFile = INVALID_HANDLE_VALUE;
        }
        DeleteCriticalSection(&logLock);
        initialized = false;
    }
};

std::string SwillLogger::logFilePath = "";
HANDLE SwillLogger::hLogFile = INVALID_HANDLE_VALUE;
CRITICAL_SECTION SwillLogger::logLock = {};
bool SwillLogger::initialized = false;

// ============================================================================
// Privilege Management
// ============================================================================
class SwillPrivileges {
public:
    static bool EnableDebugPrivilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tp;
        LUID luid;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            SwillLogger::LogF("[!] OpenProcessToken failed: %d", GetLastError());
            return false;
        }

        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            CloseHandle(hToken);
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        bool success = (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
        CloseHandle(hToken);
        
        if (success) SwillLogger::Log("[+] SeDebugPrivilege enabled");
        else SwillLogger::Log("[!] SeDebugPrivilege failed");
        return success;
    }

    static bool GrantProcessWriteAccess(DWORD pid) {
        SwillLogger::LogF("[*] Granting write access to PID %d", pid);

        HANDLE hProcess = OpenProcess(WRITE_DAC | READ_CONTROL, FALSE, pid);
        if (!hProcess) return false;

        PACL pOldDacl = NULL;
        PSECURITY_DESCRIPTOR pSD = NULL;
        if (GetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                            NULL, NULL, &pOldDacl, NULL, &pSD) != ERROR_SUCCESS) {
            CloseHandle(hProcess);
            return false;
        }

        EXPLICIT_ACCESSW ea = {};
        ea.grfAccessPermissions = PROCESS_ALL_ACCESS;
        ea.grfAccessMode = GRANT_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
        ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
        ea.Trustee.ptstrName = (LPWSTR)L"CURRENT_USER";

        PACL pNewDacl = NULL;
        if (SetEntriesInAclW(1, &ea, pOldDacl, &pNewDacl) == ERROR_SUCCESS) {
            SetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                          NULL, NULL, pNewDacl, NULL);
            LocalFree(pNewDacl);
            SwillLogger::Log("[+] Process DACL patched");
        }

        LocalFree(pSD);
        CloseHandle(hProcess);
        return true;
    }
};

// ============================================================================
// Process Utilities
// ============================================================================
class SwillProcess {
public:
    static DWORD GetProcessIdByName(const wchar_t* name) {
        DWORD pid = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, name) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return pid;
    }

    static DWORD GetFirstThreadId(DWORD pid) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        THREADENTRY32 te = { sizeof(te) };
        DWORD tid = 0;
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) {
                    tid = te.th32ThreadID;
                    break;
                }
            } while (Thread32NextW(snap, &te));
        }
        CloseHandle(snap);
        return tid;
    }

    static bool IsProcessRunning(DWORD pid) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!hProc) return false;
        
        DWORD exitCode;
        bool running = GetExitCodeProcess(hProc, &exitCode) && (exitCode == STILL_ACTIVE);
        CloseHandle(hProc);
        return running;
    }
};

// ============================================================================
// Memory Operations with Syscall Support
// ============================================================================
class SwillMemory {
private:
    static DWORD GetSyscallNumber(const char* name) {
        wchar_t ntdllPath[MAX_PATH];
        GetSystemDirectoryW(ntdllPath, MAX_PATH);
        wcscat_s(ntdllPath, MAX_PATH, L"\\ntdll.dll");

        DWORD sysnoDisk = 0;
        HANDLE hFile = CreateFileW(ntdllPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            HANDLE hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
            CloseHandle(hFile);
            
            if (hMap) {
                BYTE* pBase = (BYTE*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
                CloseHandle(hMap);
                
                if (pBase) {
                    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pBase;
                    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pBase + pDos->e_lfanew);
                    DWORD expRva = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                    PIMAGE_EXPORT_DIRECTORY pExp = (PIMAGE_EXPORT_DIRECTORY)(pBase + expRva);
                    
                    DWORD* names = (DWORD*)(pBase + pExp->AddressOfNames);
                    WORD* ordinals = (WORD*)(pBase + pExp->AddressOfNameOrdinals);
                    DWORD* funcs = (DWORD*)(pBase + pExp->AddressOfFunctions);
                    
                    for (DWORD i = 0; i < pExp->NumberOfNames; i++) {
                        if (strcmp((char*)(pBase + names[i]), name) == 0) {
                            BYTE* fn = pBase + funcs[ordinals[i]];
                            if (fn[0] == 0xB8) sysnoDisk = *(DWORD*)(fn + 1);
                            break;
                        }
                    }
                    UnmapViewOfFile(pBase);
                }
            }
        }

        if (sysnoDisk) return sysnoDisk;

        HMODULE h = GetModuleHandleA("ntdll.dll");
        if (!h) return 0;
        
        BYTE* fn = (BYTE*)GetProcAddress(h, name);
        if (!fn) return 0;
        if (fn[0] == 0xB8) return *(DWORD*)(fn + 1);
        for (int i = 1; i < 32; i++)
            if (fn[i] == 0xB8) return *(DWORD*)(fn + i + 1);
        return 0;
    }

    static void* MakeSyscallStub(DWORD sysno, BYTE argCount) {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        const char* tmpl = (argCount <= 5) ? "ZwWriteVirtualMemory" : "ZwAllocateVirtualMemory";
        BYTE* pSrc = (BYTE*)GetProcAddress(hNtdll, tmpl);
        if (!pSrc) return NULL;

        BYTE* mem = (BYTE*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!mem) return NULL;
        
        memcpy(mem, pSrc, 32);
        if (mem[0] == 0xB8) {
            *(DWORD*)(mem + 1) = sysno;
            return mem;
        }
        VirtualFree(mem, 0, MEM_RELEASE);
        return NULL;
    }

public:
    static LPVOID DirectVirtualAllocEx(HANDLE hProcess, SIZE_T size, ULONG protect) {
        static DWORD sysno = GetSyscallNumber("NtAllocateVirtualMemory");
        if (sysno) {
            typedef NTSTATUS(__stdcall* RawAllocFn)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
            RawAllocFn pfn = (RawAllocFn)MakeSyscallStub(sysno, 6);
            if (pfn) {
                PVOID base = NULL;
                SIZE_T regionSize = size;
                NTSTATUS st = pfn(hProcess, &base, 0, &regionSize, MEM_COMMIT | MEM_RESERVE, protect);
                if (st == 0 && base) return base;
            }
        }
        return VirtualAllocEx(hProcess, NULL, size, MEM_COMMIT | MEM_RESERVE, protect);
    }

    static bool DirectWriteProcessMemory(HANDLE hProcess, LPVOID lpBase, LPCVOID lpBuf, SIZE_T nSize) {
        SIZE_T wr = 0;
        return WriteProcessMemory(hProcess, lpBase, lpBuf, nSize, &wr) != FALSE;
    }
};
