#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <psapi.h>

typedef NTSTATUS(NTAPI* pNtAllocateVirtualMemory)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);

class SwillInjector {
public:
    static bool SetDebugPrivilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tp;
        LUID luid;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return false;
        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) { CloseHandle(hToken); return false; }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        bool result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        CloseHandle(hToken);
        return result && (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
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

        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProcess) {
            DWORD err = GetLastError();
            writeLog("[!] OpenProcess failed. Error: " + std::to_string(err));
            return false;
        }
        writeLog("[->] OpenProcess OK.");

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
                if (status == 0) {
                    pRemotePath = baseAddr;
                    writeLog("[->] NtAllocateVirtualMemory OK.");
                } else {
                    writeLog("[!] NtAllocateVirtualMemory failed. Status: 0x" + std::to_string(status));
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
            VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
        writeLog("[->] CreateRemoteThread OK. Waiting...");

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

        HHOOK hHook = SetWindowsHookEx(WH_GETMESSAGE, hookProc, hDll, 0);
        if (!hHook) {
            writeLog("[!] SetWindowsHookEx failed. Error: " + std::to_string(GetLastError()));
            FreeLibrary(hDll);
            return false;
        }
        writeLog("[->] SetWindowsHookEx set globally. Waiting for injection...");

        // Ждём, пока DLL загрузится в gta_sa.exe (до 10 сек)
        bool injected = false;
        for (int i = 0; i < 50; ++i) {
            Sleep(200);
            // Проверяем, загружена ли DLL в целевой процесс
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProcess) {
                HMODULE hMods[1024];
                DWORD cbNeeded;
                if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
                    for (unsigned int j = 0; j < (cbNeeded / sizeof(HMODULE)); j++) {
                        char modName[MAX_PATH];
                        if (GetModuleFileNameExA(hProcess, hMods[j], modName, sizeof(modName))) {
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
                CloseHandle(hProcess);
            }
            if (injected) break;
        }

        // Снимаем глобальный хук (DLL останется в gta_sa.exe)
        UnhookWindowsHookEx(hHook);
        writeLog("[->] Unhooked. DLL presence in target: " + std::string(injected ? "YES" : "NO"));
        FreeLibrary(hDll);
        return injected;
    }
};




