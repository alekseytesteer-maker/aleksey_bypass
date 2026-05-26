#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <psapi.h>
#include <sddl.h>
#include <vector>
#include <shellapi.h>

typedef NTSTATUS(NTAPI* pNtAllocateVirtualMemory)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef HANDLE(NTAPI* pRtlCreateUserThread)(HANDLE, PVOID, BOOL, ULONG, ULONG, ULONG, PVOID, PVOID, PVOID, PVOID);

class SwillInjector {
public:
    // Проверка, запущен ли процесс с повышенными правами
    static bool IsProcessElevated() {
        BOOL fIsElevated = FALSE;
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            TOKEN_ELEVATION elevation;
            DWORD dwSize;
            if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
                fIsElevated = elevation.TokenIsElevated;
            }
            CloseHandle(hToken);
        }
        return fIsElevated == TRUE;
    }

    // Попытка получить токен администратора и перезапустить процесс
    static bool RunAsAdmin(const std::wstring& exePath) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = exePath.c_str();
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd = NULL;
        sei.nShow = SW_SHOWNORMAL;

        if (!ShellExecuteExW(&sei)) {
            return false;
        }
        return true;
    }

    static bool SetDebugPrivilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tp;
        LUID luid;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }
        
        if (!LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &luid)) { 
            CloseHandle(hToken); 
            return false; 
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        bool result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL) != FALSE;
        DWORD lastError = GetLastError();
        CloseHandle(hToken);
        
        // Возвращаем true, даже если привилегия уже была установлена или частично применена
        return result && (lastError != ERROR_NOT_ALL_ASSIGNED);
    }

    static DWORD GetProcessId(const std::wstring& procName) {
        DWORD pid = 0;
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (procName == pe.szExeFile) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
        return pid;
    }

    static bool InjectDLL(DWORD pid, const std::string& dllPath, std::ofstream& log) {
        auto writeLog = [&](const std::string& s) {
            log << s << std::endl;
            log.flush();
            std::cout << s << std::endl;
        };

        // Попытка 1: Стандартный OpenProcess с PROCESS_ALL_ACCESS
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        
        // Если не удалось, пробуем более специфичные права
        if (!hProcess) {
            DWORD err = GetLastError();
            writeLog("[!] OpenProcess(PROCESS_ALL_ACCESS) failed. Error: " + std::to_string(err) + ". Trying reduced rights...");
            
            // Пробуем с минимально необходимыми правами
            DWORD accessRights = PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION;
            hProcess = OpenProcess(accessRights, FALSE, pid);
            
            if (!hProcess) {
                err = GetLastError();
                writeLog("[!] OpenProcess(reduced rights) failed. Error: " + std::to_string(err));
                
                // Последняя попытка - только QUERY и VM_READ (для диагностики)
                hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                if (hProcess) {
                    writeLog("[->] OpenProcess with minimal rights OK (read-only). Injection impossible without elevation.");
                    CloseHandle(hProcess);
                    return false;
                }
                return false;
            }
            writeLog("[->] OpenProcess with reduced rights OK.");
        } else {
            writeLog("[->] OpenProcess OK.");
        }

        // Попытка выделения памяти через VirtualAllocEx
        LPVOID pRemotePath = VirtualAllocEx(hProcess, NULL, dllPath.length() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!pRemotePath) {
            DWORD err = GetLastError();
            writeLog("[!] VirtualAllocEx failed. Error: " + std::to_string(err) + ". Trying NtAllocateVirtualMemory...");

            pNtAllocateVirtualMemory NtAllocateVirtualMemory = (pNtAllocateVirtualMemory)GetProcAddress(
                GetModuleHandleA("ntdll.dll"), "NtAllocateVirtualMemory");
            if (NtAllocateVirtualMemory) {
                PVOID baseAddr = NULL;
                SIZE_T size = dllPath.length() + 1;
                NTSTATUS status = NtAllocateVirtualMemory(hProcess, &baseAddr, 0, &size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (status == 0 || status == 0x40000000) { // STATUS_SUCCESS или STATUS_PENDING
                    pRemotePath = baseAddr;
                    writeLog("[->] NtAllocateVirtualMemory OK.");
                } else {
                    writeLog("[!] NtAllocateVirtualMemory failed. Status: 0x" + std::to_string(status) + 
                             " (Likely ACCESS_DENIED due to protection mechanisms).");
                    CloseHandle(hProcess);
                    return false;
                }
            } else {
                writeLog("[!] NtAllocateVirtualMemory not found.");
                CloseHandle(hProcess);
                return false;
            }
        } else {
            writeLog("[->] VirtualAllocEx OK.");
        }

        if (!WriteProcessMemory(hProcess, pRemotePath, dllPath.c_str(), dllPath.length() + 1, NULL)) {
            DWORD err = GetLastError();
            writeLog("[!] WriteProcessMemory failed. Error: " + std::to_string(err));
            VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
        writeLog("[->] WriteProcessMemory OK.");

        PTHREAD_START_ROUTINE pLoadLibrary = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
        if (!pLoadLibrary) {
            writeLog("[!] GetProcAddress(LoadLibraryA) failed.");
            VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
        writeLog("[->] GetProcAddress OK.");

        HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pLoadLibrary, pRemotePath, 0, NULL);
        if (!hThread) {
            DWORD err = GetLastError();
            writeLog("[!] CreateRemoteThread failed. Error: " + std::to_string(err));
            
            // Альтернатива: RtlCreateUserThread
            writeLog("[*] Trying RtlCreateUserThread as fallback...");
            pRtlCreateUserThread RtlCreateUserThread = (pRtlCreateUserThread)GetProcAddress(
                GetModuleHandleA("ntdll.dll"), "RtlCreateUserThread");
            
            if (RtlCreateUserThread) {
                HANDLE hAltThread = NULL;
                NTSTATUS status = RtlCreateUserThread(hProcess, NULL, FALSE, 0, 0, 0, pLoadLibrary, pRemotePath, &hAltThread, NULL);
                if (status == 0 || status == 0x40000000) {
                    hThread = hAltThread;
                    writeLog("[->] RtlCreateUserThread OK.");
                } else {
                    writeLog("[!] RtlCreateUserThread failed. Status: 0x" + std::to_string(status));
                }
            }
            
            if (!hThread) {
                VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
                CloseHandle(hProcess);
                return false;
            }
        }
        writeLog("[->] Thread created. Waiting...");

        WaitForSingleObject(hThread, INFINITE);
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hThread);
        CloseHandle(hProcess);
        writeLog("[->] Thread finished. Injection complete.");
        return true;
    }

    static bool InjectViaHook(DWORD pid, const std::string& dllPath, std::ofstream& log) {
        auto writeLog = [&](const std::string& s) {
            log << s << std::endl;
            log.flush();
            std::cout << s << std::endl;
        };

        writeLog("[*] Attempting SetWindowsHookEx injection...");
        
        // Проверяем уровень целостности целевого процесса
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProcess) {
            HANDLE hToken = NULL;
            if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
                TOKEN_MANDATORY_LABEL tml;
                DWORD dwSize = 0;
                if (GetTokenInformation(hToken, TokenIntegrityLevel, NULL, 0, &dwSize) || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    std::vector<BYTE> buffer(dwSize);
                    if (GetTokenInformation(hToken, TokenIntegrityLevel, buffer.data(), dwSize, &dwSize)) {
                        tml = *reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer.data());
                        DWORD integrityLevel = *GetSidSubAuthority(tml.Label.Sid, (DWORD)(UCHAR)(*GetSidSubAuthorityCount(tml.Label.Sid) - 1));
                        
                        std::string integrityStr = "Unknown";
                        if (integrityLevel < SECURITY_MANDATORY_MEDIUM_RID) integrityStr = "Low";
                        else if (integrityLevel < SECURITY_MANDATORY_HIGH_RID) integrityStr = "Medium";
                        else if (integrityLevel < SECURITY_MANDATORY_SYSTEM_RID) integrityStr = "High";
                        else integrityStr = "System";
                        
                        writeLog("[i] Target process integrity level: " + integrityStr);
                        
                        // Если целевой процесс имеет высокий уровень целостности, а мы нет - предупреждаем
                        if (integrityLevel >= SECURITY_MANDATORY_HIGH_RID && !IsProcessElevated()) {
                            writeLog("[!] WARNING: Target has HIGH integrity, but loader is not elevated. Hook injection will likely fail due to UIPI.");
                        }
                    }
                }
                CloseHandle(hToken);
            }
            CloseHandle(hProcess);
        }

        HMODULE hDll = LoadLibraryA(dllPath.c_str());
        if (!hDll) {
            writeLog("[!] LoadLibraryA(local) failed. Error: " + std::to_string(GetLastError()));
            return false;
        }
        writeLog("[->] Local LoadLibraryA OK.");

        HOOKPROC hookProc = (HOOKPROC)GetProcAddress(hDll, "SwillHookProc");
        if (!hookProc) hookProc = (HOOKPROC)GetProcAddress(hDll, "SwillHookProc@12");
        if (!hookProc) hookProc = (HOOKPROC)GetProcAddress(hDll, MAKEINTRESOURCE(1));
        if (!hookProc) {
            writeLog("[!] GetProcAddress(SwillHookProc) failed. Error: " + std::to_string(GetLastError()));
            FreeLibrary(hDll);
            return false;
        }
        writeLog("[->] Hook proc found.");

        // Пробуем WH_GETMESSAGE, затем WH_CALLWNDPROC
        HHOOK hHook = SetWindowsHookEx(WH_GETMESSAGE, hookProc, hDll, 0);
        if (!hHook) {
            writeLog("[!] SetWindowsHookEx(WH_GETMESSAGE) failed. Error: " + std::to_string(GetLastError()));
            writeLog("[*] Trying WH_CALLWNDPROC...");
            hHook = SetWindowsHookEx(WH_CALLWNDPROC, hookProc, hDll, 0);
        }
        
        if (!hHook) {
            writeLog("[!] SetWindowsHookEx(WH_CALLWNDPROC) failed. Error: " + std::to_string(GetLastError()));
            writeLog("[!] HINT: Run the loader as Administrator to bypass UIPI restrictions.");
            FreeLibrary(hDll);
            return false;
        }
        writeLog("[->] SetWindowsHookEx set globally. Waiting for injection...");

        // Ждём, пока DLL загрузится в gta_sa.exe (до 10 сек)
        bool injected = false;
        for (int i = 0; i < 50; ++i) {
            Sleep(200);
            // Проверяем, загружена ли DLL в целевой процесс
            HANDLE hProcessCheck = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProcessCheck) {
                HMODULE hMods[1024];
                DWORD cbNeeded;
                if (EnumProcessModules(hProcessCheck, hMods, sizeof(hMods), &cbNeeded)) {
                    for (unsigned int j = 0; j < (cbNeeded / sizeof(HMODULE)); j++) {
                        char modName[MAX_PATH];
                        if (GetModuleFileNameExA(hProcessCheck, hMods[j], modName, sizeof(modName))) {
                            std::string name(modName);
                            size_t pos = name.rfind(static_cast<char>(92));
                            if (pos == std::string::npos) pos = name.rfind('/');
                            std::string base = (pos == std::string::npos) ? name : name.substr(pos + 1);
                            if (base == "SwillPayload.dll") {
                                injected = true;
                                break;
                            }
                        }
                    }
                }
                CloseHandle(hProcessCheck);
            }
            if (injected) break;
        }

        // Снимаем глобальный хук (DLL останется в gta_sa.exe)
        UnhookWindowsHookEx(hHook);
        writeLog("[->] Unhooked. DLL presence in target: " + std::string(injected ? "YES" : "NO"));
        if (!injected) {
            writeLog("[!] HINT: Ensure target process has a message queue and runs at same or lower integrity level.");
        }
        FreeLibrary(hDll);
        return injected;
    }
};




