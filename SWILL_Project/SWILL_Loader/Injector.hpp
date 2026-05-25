#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

class SwillInjector {
public:
    // Включение привилегии отладки для доступа к защищенным процессам
    static bool SetDebugPrivilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tp;
        LUID luid;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            std::cerr << "[!] OpenProcessToken failed: " << GetLastError() << std::endl;
            return false;
        }
        
        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            std::cerr << "[!] LookupPrivilegeValue failed: " << GetLastError() << std::endl;
            CloseHandle(hToken);
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        bool result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        DWORD error = GetLastError();
        CloseHandle(hToken);
        
        return result && (error != ERROR_NOT_ALL_ASSIGNED);
    }

    // Поиск PID процесса по имени
    static DWORD GetProcessId(const std::wstring& procName) {
        DWORD pid = 0;
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, procName.c_str()) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
        return pid;
    }

    // Инъекция DLL в целевой процесс
    static bool InjectDLL(DWORD pid, const std::string& dllPath) {
        // Используем минимально необходимые права для снижения детектируемости
        HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!hProcess) {
            std::cerr << "[!] Ошибка OpenProcess: " << GetLastError() << std::endl;
            return false;
        }

        // Выделяем память под путь к DLL
        SIZE_T pathSize = dllPath.length() + 1;
        LPVOID pRemotePath = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!pRemotePath) {
            std::cerr << "[!] Ошибка VirtualAllocEx: " << GetLastError() << std::endl;
            CloseHandle(hProcess);
            return false;
        }

        // Записываем путь к DLL в память целевого процесса
        if (!WriteProcessMemory(hProcess, pRemotePath, dllPath.c_str(), pathSize, NULL)) {
            std::cerr << "[!] Ошибка WriteProcessMemory: " << GetLastError() << std::endl;
            VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        // Получаем адрес LoadLibraryA
        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
        if (!hKernel32) {
            VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        PTHREAD_START_ROUTINE pLoadLibrary = (PTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA");
        if (!pLoadLibrary) {
            VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        // Создаем удаленный поток
        HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pLoadLibrary, pRemotePath, 0, NULL);
        if (!hThread) {
            std::cerr << "[!] Ошибка CreateRemoteThread: " << GetLastError() << std::endl;
            VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        // Ожидаем завершения загрузки DLL
        WaitForSingleObject(hThread, INFINITE);
        
        // Получаем код возврата потока (успешно ли загрузилась DLL)
        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);
        
        // Очищаем память и закрываем_handles
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hThread);
        CloseHandle(hProcess);

        return (exitCode != 0);
    }
};
