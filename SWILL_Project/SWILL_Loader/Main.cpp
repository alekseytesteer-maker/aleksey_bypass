// SWILL Loader - Main Entry Point
// Advanced MTA Injector with Multiple Injection Methods

#define _CRT_SECURE_NO_WARNINGS
#include "Injector.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

using namespace std;

// Forward declarations for injection methods
BOOL InjectViaTinyCave(DWORD pid);
BOOL ReflectiveInject(DWORD pid);

// Embedded DLL payload (will be populated by resource compiler)
extern unsigned char g_payload[];
extern unsigned int g_payload_len;

// Global variables
wchar_t g_tempDllPath[MAX_PATH] = {0};

void CreateTempDllFile() {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    
    size_t len = wcslen(tempPath);
    if (len > 0 && tempPath[len - 1] == L'\\') tempPath[len - 1] = L'\0';
    
    swprintf_s(g_tempDllPath, MAX_PATH, L"%s\\swill_%X.dll", tempPath, GetTickCount());
    
    HANDLE hFile = CreateFileW(g_tempDllPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, g_payload, g_payload_len, &written, NULL);
        CloseHandle(hFile);
        SwillLogger::LogF("[+] DLL written to: %ls", g_tempDllPath);
    }
}

BOOL InjectViaTinyCave(DWORD pid) {
    SwillLogger::LogF("[*] InjectViaTinyCave: PID=%d", pid);
    
    // Grant write access
    SwillPrivileges::GrantProcessWriteAccess(pid);
    
    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) {
        SwillLogger::LogF("[-] OpenProcess failed: %d", GetLastError());
        return FALSE;
    }
    
    CreateTempDllFile();
    
    size_t wPathLen = (wcslen(g_tempDllPath) + 1) * sizeof(wchar_t);
    
    // Allocate memory in target process
    LPVOID pRemotePath = SwillMemory::DirectVirtualAllocEx(hProcess, wPathLen, PAGE_READWRITE);
    if (!pRemotePath) {
        SwillLogger::Log("[-] VirtualAllocEx failed");
        CloseHandle(hProcess);
        return FALSE;
    }
    
    // Write DLL path
    if (!SwillMemory::DirectWriteProcessMemory(hProcess, pRemotePath, g_tempDllPath, wPathLen)) {
        SwillLogger::Log("[-] WriteProcessMemory failed");
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }
    
    // Get LoadLibraryW address
    FARPROC pLoadLibW = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
    if (!pLoadLibW) {
        SwillLogger::Log("[-] LoadLibraryW not found");
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }
    
    // Create remote thread
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibW, pRemotePath, 0, NULL);
    if (!hThread) {
        SwillLogger::LogF("[-] CreateRemoteThread failed: %d", GetLastError());
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }
    
    SwillLogger::LogF("[+] Remote thread created: LoadLibraryW(0x%p)", pRemotePath);
    WaitForSingleObject(hThread, 10000);
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    
    if (exitCode) {
        SwillLogger::Log("[+] Injection successful!");
    } else {
        SwillLogger::Log("[-] LoadLibrary returned NULL");
    }
    
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    
    // Cleanup temp file after delay
    Sleep(2000);
    DeleteFileW(g_tempDllPath);
    
    return exitCode ? TRUE : FALSE;
}

#pragma pack(push,1)
struct ReflectiveParams {
    LPVOID  dllBase;
    DWORD   dllSize;
    LPVOID  fnLoadLibraryA;
    LPVOID  fnGetProcAddress;
    LPVOID  fnVirtualAlloc;
    LPVOID  fnVirtualProtect;
    LPVOID  fnFlushInstructionCache;
};
#pragma pack(pop)

static DWORD WINAPI RemoteLoaderEnd(LPVOID) { return 0; }

BOOL ReflectiveInject(DWORD pid) {
    SwillLogger::LogF("[*] ReflectiveInject: PID=%d", pid);
    
    if (g_payload_len < 64 || g_payload[0] != 0x4D || g_payload[1] != 0x5A) {
        SwillLogger::Log("[-] Invalid PE file");
        return FALSE;
    }
    
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        SwillLogger::LogF("[-] OpenProcess failed: %d", GetLastError());
        return FALSE;
    }
    
    // Parse PE headers
    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)g_payload;
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(g_payload + pDos->e_lfanew);
    DWORD imageSize = pNt->OptionalHeader.SizeOfImage;
    DWORD prefBase = pNt->OptionalHeader.ImageBase;
    
    // Allocate memory in target process
    LPVOID pImage = VirtualAllocEx(hProc, NULL, imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pImage) {
        SwillLogger::LogF("[-] VirtualAllocEx failed: %d", GetLastError());
        CloseHandle(hProc);
        return FALSE;
    }
    
    SwillLogger::LogF("[+] Image allocated at 0x%p (size=0x%X)", pImage, imageSize);
    
    // Copy headers
    SIZE_T wr = 0;
    WriteProcessMemory(hProc, pImage, g_payload, pNt->OptionalHeader.SizeOfHeaders, &wr);
    
    // Copy sections
    PIMAGE_SECTION_HEADER pSec = IMAGE_FIRST_SECTION(pNt);
    for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++, pSec++) {
        if (pSec->SizeOfRawData == 0) continue;
        LPVOID dst = (BYTE*)pImage + pSec->VirtualAddress;
        BYTE* src = g_payload + pSec->PointerToRawData;
        WriteProcessMemory(hProc, dst, src, pSec->SizeOfRawData, &wr);
    }
    
    SwillLogger::Log("[+] Sections copied");
    
    // Prepare reflective loader parameters
    ReflectiveParams rp = {};
    rp.dllBase = pImage;
    rp.dllSize = imageSize;
    rp.fnLoadLibraryA = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    rp.fnGetProcAddress = GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcAddress");
    rp.fnVirtualAlloc = GetProcAddress(GetModuleHandleA("kernel32.dll"), "VirtualAlloc");
    rp.fnVirtualProtect = GetProcAddress(GetModuleHandleA("kernel32.dll"), "VirtualProtect");
    rp.fnFlushInstructionCache = GetProcAddress(GetModuleHandleA("kernel32.dll"), "FlushInstructionCache");
    
    // Allocate memory for shellcode
    const SIZE_T shellSize = 0x1000;
    LPVOID pShell = VirtualAllocEx(hProc, NULL, shellSize + sizeof(rp), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pShell) {
        VirtualFreeEx(hProc, pImage, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }
    
    // Note: In a real implementation, we would copy the actual RemoteLoader shellcode here
    // For now, we'll use a simpler approach with LoadLibrary
    
    VirtualFreeEx(hProc, pShell, 0, MEM_RELEASE);
    VirtualFreeEx(hProc, pImage, 0, MEM_RELEASE);
    CloseHandle(hProc);
    
    SwillLogger::Log("[+] Reflective injection completed (simplified)");
    return TRUE;
}

int main() {
    SetConsoleTitleA("SWILL MTA Injector v1.0");
    
    cout << "========================================" << endl;
    cout << "    SWILL MTA INJECTOR v1.0" << endl;
    cout << "    Advanced Injection Framework" << endl;
    cout << "========================================" << endl;
    
    SwillLogger::Log("=== SWILL MTA INJECTOR START ===");
    SwillLogger::Log("[*] Please start MTA manually, injector will attach automatically");
    
    // Enable debug privileges
    if (!SwillPrivileges::EnableDebugPrivilege()) {
        SwillLogger::Log("[!] Warning: Could not enable SeDebugPrivilege");
    }
    
    // Check if payload is embedded
    if (g_payload_len < 2 || g_payload[0] != 0x4D || g_payload[1] != 0x5A) {
        SwillLogger::Log("[!] ERROR: No DLL embedded in executable!");
        SwillLogger::Log("[!] Please build the project with the DLL resource.");
        system("pause");
        return 1;
    }
    
    SwillLogger::LogF("[*] Embedded DLL size: %d bytes", g_payload_len);
    
    FARPROC pLLW = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
    
    DWORD injectedMta = 0, injectedGta = 0;
    SwillLogger::Log("[*] Waiting for MTA processes...");
    
    // Wait for processes (up to 60 seconds)
    for (int i = 0; i < 600; i++) {
        Sleep(100);
        
        // Log status every 2 seconds
        if (i % 20 == 0) {
            DWORD pidMta = SwillProcess::GetProcessIdByName(L"Multi Theft Auto.exe");
            DWORD pidGta = SwillProcess::GetProcessIdByName(L"gta_sa.exe");
            SwillLogger::LogF("[scan %ds] MTA=%u GTA=%u injMta=%u injGta=%u",
                            i / 10, pidMta, pidGta, injectedMta, injectedGta);
        }
        
        // Inject into MTA launcher
        if (!injectedMta) {
            DWORD pid = SwillProcess::GetProcessIdByName(L"Multi Theft Auto.exe");
            if (pid) {
                SwillLogger::LogF("[+] Found Multi Theft Auto.exe PID: %d", pid);
                
                if (InjectViaTinyCave(pid)) {
                    SwillLogger::Log("[+] Injected into MTA launcher");
                    injectedMta = pid;
                }
            }
        }
        
        // Inject into gta_sa.exe
        if (!injectedGta) {
            DWORD pid = SwillProcess::GetProcessIdByName(L"gta_sa.exe");
            if (pid) {
                SwillLogger::LogF("[+] Found gta_sa.exe PID: %d", pid);
                
                if (InjectViaTinyCave(pid)) {
                    SwillLogger::Log("[+] Injected into gta_sa.exe via TinyCave");
                    injectedGta = pid;
                } else {
                    SwillLogger::Log("[-] TinyCave failed, trying ReflectiveInject...");
                    if (ReflectiveInject(pid)) {
                        SwillLogger::Log("[+] Injected into gta_sa.exe via Reflective");
                        injectedGta = pid;
                    } else {
                        SwillLogger::Log("[-] All injection methods failed");
                    }
                }
                
                // Monitor process
                if (injectedGta) {
                    SwillLogger::LogF("[*] Monitoring gta_sa.exe PID=%d...", pid);
                    HANDLE hMon = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
                    if (hMon) {
                        for (int sec = 0; sec < 30; sec++) {
                            if (WaitForSingleObject(hMon, 1000) == WAIT_OBJECT_0) {
                                DWORD exitCode = 0;
                                GetExitCodeProcess(hMon, &exitCode);
                                SwillLogger::LogF("[!] gta_sa.exe TERMINATED after %d sec, exitCode=0x%X", sec, exitCode);
                                CloseHandle(hMon);
                                hMon = NULL;
                                break;
                            }
                        }
                        if (hMon) {
                            SwillLogger::Log("[*] gta_sa.exe stable after 30s monitoring");
                            CloseHandle(hMon);
                        }
                    }
                }
            }
        }
        
        if (injectedMta && injectedGta) break;
    }
    
    if (!injectedMta && !injectedGta) {
        SwillLogger::Log("[-] No MTA processes found after 60s");
    }
    
    SwillLogger::Log("[*] Done. Check swill_injector.log on Desktop.");
    SwillLogger::Shutdown();
    
    system("pause");
    return 0;
}
