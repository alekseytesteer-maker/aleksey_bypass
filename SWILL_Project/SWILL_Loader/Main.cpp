#define _CRT_SECURE_NO_WARNINGS
#include "Injector.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")

// Ресурс DLL (будет внедрен через Resource.rc)
extern unsigned char g_payload[];
extern unsigned int g_payload_len;

// Логгер на рабочий стол
void LogToDesktop(const char* msg) {
    char path[MAX_PATH];
    GetEnvironmentVariableA("USERPROFILE", path, MAX_PATH);
    strcat_s(path, MAX_PATH, "\\Desktop\\swill_injector_log.txt");
    
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile != INVALID_HANDLE_VALUE) {
        SYSTEMTIME st; 
        GetLocalTime(&st);
        char timeBuf[64];
        sprintf_s(timeBuf, sizeof(timeBuf), "[%02d:%02d:%02d.%03d] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        
        SetFilePointer(hFile, 0, NULL, FILE_END);
        DWORD written;
        WriteFile(hFile, timeBuf, (DWORD)strlen(timeBuf), &written, NULL);
        WriteFile(hFile, msg, (DWORD)strlen(msg), &written, NULL);
        WriteFile(hFile, "\r\n", 2, &written, NULL);
        CloseHandle(hFile);
    }
    
    std::cout << msg << std::endl;
}

void ClearDesktopLog() {
    char path[MAX_PATH];
    GetEnvironmentVariableA("USERPROFILE", path, MAX_PATH);
    strcat_s(path, MAX_PATH, "\\Desktop\\swill_injector_log.txt");
    
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
}

void LogToDesktopF(const char* fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    LogToDesktop(msg);
}

// Получение syscall номера из disk ntdll
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
    
    if (sysnoDisk) {
        LogToDesktopF("[*] %s syscall (disk): 0x%X", name, sysnoDisk);
        return sysnoDisk;
    }

    HMODULE h = GetModuleHandleA("ntdll.dll");
    if (!h) return 0;
    BYTE* fn = (BYTE*)GetProcAddress(h, name);
    if (!fn) return 0;
    if (fn[0] == 0xB8) return *(DWORD*)(fn + 1);
    for (int i = 1; i < 32; i++)
        if (fn[i] == 0xB8) return *(DWORD*)(fn + i + 1);
    return 0;
}

typedef NTSTATUS(__stdcall* RawAllocFn)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);

static void* MakeSyscallStub(DWORD sysno, const char* tmpl) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    BYTE* pSrc = (BYTE*)GetProcAddress(hNtdll, tmpl);
    if (!pSrc) { 
        LogToDesktopF("[-] %s not found", tmpl); 
        return NULL; 
    }

    BYTE* mem = (BYTE*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return NULL;
    memcpy(mem, pSrc, 32);

    if (mem[0] == 0xB8) {
        *(DWORD*)(mem + 1) = sysno;
        LogToDesktopF("[*] Stub from %s, sysno=0x%X", tmpl, sysno);
        return mem;
    }
    
    VirtualFree(mem, 0, MEM_RELEASE);
    return NULL;
}

static LPVOID DirectVirtualAllocEx(HANDLE hProcess, SIZE_T size, ULONG protect) {
    static RawAllocFn pfn = NULL;
    if (!pfn) {
        DWORD sysno = GetSyscallNumber("NtAllocateVirtualMemory");
        if (sysno) pfn = (RawAllocFn)MakeSyscallStub(sysno, "ZwAllocateVirtualMemory");
    }
    
    if (pfn) {
        PVOID base = NULL;
        SIZE_T regionSize = size;
        NTSTATUS st = pfn(hProcess, &base, 0, &regionSize, MEM_COMMIT | MEM_RESERVE, protect);
        if (st == 0) return base;
        LogToDesktopF("[!] NtAllocVM NTSTATUS=0x%X", st);
    }
    
    return VirtualAllocEx(hProcess, NULL, size, MEM_COMMIT | MEM_RESERVE, protect);
}

void EnableDebugPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;
    LUID luid;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        }
        CloseHandle(hToken);
    }
}

DWORD GetProcessIdByName(const wchar_t* name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
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
    }
    return pid;
}

bool InjectDLL_Advanced(DWORD pid, const wchar_t* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) { 
        LogToDesktopF("[-] OpenProcess failed: %d", GetLastError()); 
        return false; 
    }
    
    LogToDesktopF("[+] Process opened: PID %d", pid);

    size_t pathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    
    // Выделяем память через syscall stub
    LPVOID pRemotePath = DirectVirtualAllocEx(hProcess, pathLen, PAGE_READWRITE);
    if (!pRemotePath) {
        LogToDesktopF("[-] VirtualAllocEx failed: %d", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pRemotePath, dllPath, pathLen, NULL)) {
        LogToDesktopF("[-] WriteProcessMemory failed: %d", GetLastError());
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    FARPROC pLoadLibW = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
    if (!pLoadLibW) {
        LogToDesktop("[-] LoadLibraryW not found");
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibW, pRemotePath, 0, NULL);
    
    if (!hThread) {
        LogToDesktopF("[-] CreateRemoteThread failed: %d", GetLastError());
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    LogToDesktopF("[+] Remote thread created at 0x%p", pRemotePath);
    WaitForSingleObject(hThread, 10000);
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    
    if (exitCode) {
        LogToDesktop("[+++++] Injection successful!");
        return true;
    } else {
        LogToDesktop("[-] LoadLibraryW returned NULL");
        return false;
    }
}

int main() {
    SetConsoleTitleA("SWILL MTA Injector v" SWILL_VERSION);
    ClearDesktopLog();
    
    LogToDesktop("==================================================");
    LogToDesktop("       SWILL MTA INJECTOR v" SWILL_VERSION);
    LogToDesktop("==================================================");
    LogToDesktop("[*] Запустите MTA вручную - инжектор подключится автоматически");
    
    EnableDebugPrivilege();
    LogToDesktop("[+] SeDebugPrivilege enabled");

    // Проверяем наличие встроенной DLL
    if (g_payload_len < 64 || g_payload[0] != 'M' || g_payload[1] != 'Z') {
        LogToDesktop("[!] CRITICAL: No valid DLL embedded!");
        LogToDesktop("[i] Build SWILL_Payload first, then copy bin/SwillPayload.dll to SWILL_Loader/swill_payload.bin");
        system("pause");
        return 1;
    }
    
    LogToDesktopF("[*] Embedded payload size: %d bytes", g_payload_len);

    // Ждем процесс gta_sa.exe
    LogToDesktop("[*] Waiting for gta_sa.exe...");
    
    DWORD pid = 0;
    for (int i = 0; i < 600; i++) { // 60 секунд
        Sleep(100);
        
        if (i % 50 == 0) {
            DWORD checkPid = GetProcessIdByName(L"gta_sa.exe");
            LogToDesktopF("[scan %ds] gta_sa.exe = %u", i / 10, checkPid);
        }
        
        pid = GetProcessIdByName(L"gta_sa.exe");
        if (pid != 0) break;
    }

    if (pid == 0) {
        LogToDesktop("[-] gta_sa.exe not found after 60 seconds");
        LogToDesktop("[*] Please launch MTA first, then run this injector");
        system("pause");
        return 1;
    }

    LogToDesktopF("[+] Found gta_sa.exe (PID: %d)", pid);

    // Создаем временный файл DLL
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    size_t len = wcslen(tempPath);
    if (len > 0 && tempPath[len - 1] == L'\\') tempPath[len - 1] = L'\0';
    
    wchar_t dllPath[MAX_PATH];
    swprintf_s(dllPath, MAX_PATH, L"%s\\swill_%d.dll", tempPath, GetCurrentProcessId());
    
    HANDLE hFile = CreateFileW(dllPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, g_payload, g_payload_len, &written, NULL);
        CloseHandle(hFile);
        LogToDesktopF("[+] DLL written to: %ls", dllPath);
    } else {
        LogToDesktopF("[-] Failed to create temp DLL: %d", GetLastError());
        system("pause");
        return 1;
    }

    // Инжектируем
    LogToDesktop("[*] Injecting payload...");
    if (InjectDLL_Advanced(pid, dllPath)) {
        LogToDesktop("[+++++] INJECTION COMPLETED SUCCESSFULLY!");
        LogToDesktop("[*] Press INSERT in game to open GUI menu");
    } else {
        LogToDesktop("[-] Injection failed");
    }

    // Ждем немного и удаляем временный файл
    Sleep(2000);
    DeleteFileW(dllPath);
    LogToDesktop("[*] Temp file cleaned");

    LogToDesktop("==================================================");
    LogToDesktop("[*] Check Desktop\\swill_injector_log.txt for details");
    LogToDesktop("==================================================");
    
    system("pause");
    return 0;
}
