#include "Injector.hpp"
#include <iostream>
#include <vector>

// Статические члены класса SwillLogger
std::ofstream SwillLogger::s_logFile;
std::mutex SwillLogger::s_mutex;
bool SwillLogger::s_initialized = false;

bool SwillLogger::Initialize(const char* filename) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized) return true;
    
    s_logFile.open(filename, std::ios::out | std::ios::app);
    if (!s_logFile.is_open()) {
        return false;
    }
    
    s_initialized = true;
    LogF("INFO", "=== SWILL Logger Initialized ===");
    return true;
}

void SwillLogger::Shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_logFile.is_open()) {
        LogF("INFO", "=== SWILL Logger Shutdown ===");
        s_logFile.close();
    }
    s_initialized = false;
}

void SwillLogger::Log(const char* level, const char* message) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_initialized || !s_logFile.is_open()) return;
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    s_logFile << "[" 
              << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "." << st.wMilliseconds
              << "] [" << level << "] " << message << std::endl;
    s_logFile.flush();
}

void SwillLogger::LogF(const char* level, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);
    
    Log(level, buffer);
}

void SwillLogger::Info(const char* message) { Log("INFO", message); }
void SwillLogger::Warning(const char* message) { Log("WARNING", message); }
void SwillLogger::Error(const char* message) { Log("ERROR", message); }
void SwillLogger::Success(const char* message) { Log("SUCCESS", message); }
void SwillLogger::Debug(const char* message) { Log("DEBUG", message); }

// Функции для работы с привилегиями и процессами
namespace SwillInjector {

bool SetDebugPrivilege() {
    HandleGuard hToken;
    TOKEN_PRIVILEGES tp = {};
    LUID luid;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken.h)) {
        SwillLogger::Error("Failed to open process token");
        return false;
    }
    
    if (!LookupPrivilegeValueA(NULL, SE_DEBUG_NAME, &luid)) {
        SwillLogger::Error("Failed to lookup privilege value");
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken.get(), FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        SwillLogger::Error("Failed to adjust token privileges");
        return false;
    }
    
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        SwillLogger::Error("SeDebugPrivilege not available");
        return false;
    }
    
    SwillLogger::Success("SeDebugPrivilege enabled successfully");
    return true;
}

DWORD GetProcessIdByName(const wchar_t* procName) {
    DWORD pid = 0;
    HandleGuard hSnap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    
    if (hSnap.get() == INVALID_HANDLE_VALUE) {
        SwillLogger::Error("Failed to create process snapshot");
        return 0;
    }

    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(hSnap.get(), &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, procName) == 0) {
                pid = pe.th32ProcessID;
                SwillLogger::LogF("INFO", "Found process %ls with PID: %d", procName, pid);
                break;
            }
        } while (Process32NextW(hSnap.get(), &pe));
    }
    
    return pid;
}

DWORD GetFirstThreadId(DWORD pid) {
    HandleGuard hSnap(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid));
    if (hSnap.get() == INVALID_HANDLE_VALUE) return 0;
    
    THREADENTRY32 te = { sizeof(te) };
    DWORD tid = 0;
    
    if (Thread32First(hSnap.get(), &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                tid = te.th32ThreadID;
                break;
            }
        } while (Thread32Next(hSnap.get(), &te));
    }
    
    return tid;
}

bool InjectDLL_LoadLibrary(DWORD pid, const wchar_t* dllPath) {
    HandleGuard hProcess(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
    if (!hProcess.get()) {
        SwillLogger::LogF("ERROR", "OpenProcess failed: %d", GetLastError());
        return false;
    }

    size_t pathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    
    // Выделяем память в целевом процессе
    LPVOID pRemotePath = VirtualAllocEx(hProcess.get(), NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemotePath) {
        SwillLogger::LogF("ERROR", "VirtualAllocEx failed: %d", GetLastError());
        return false;
    }

    // Записываем путь к DLL
    if (!WriteProcessMemory(hProcess.get(), pRemotePath, dllPath, pathLen, NULL)) {
        SwillLogger::LogF("ERROR", "WriteProcessMemory failed: %d", GetLastError());
        VirtualFreeEx(hProcess.get(), pRemotePath, 0, MEM_RELEASE);
        return false;
    }

    // Получаем адрес LoadLibraryW
    FARPROC pLoadLibW = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
    if (!pLoadLibW) {
        SwillLogger::Error("GetProcAddress(LoadLibraryW) failed");
        VirtualFreeEx(hProcess.get(), pRemotePath, 0, MEM_RELEASE);
        return false;
    }

    // Создаем удаленный поток
    HandleGuard hThread(CreateRemoteThread(hProcess.get(), NULL, 0, 
        (LPTHREAD_START_ROUTINE)pLoadLibW, pRemotePath, 0, NULL));
    
    if (!hThread.get()) {
        SwillLogger::LogF("ERROR", "CreateRemoteThread failed: %d", GetLastError());
        VirtualFreeEx(hProcess.get(), pRemotePath, 0, MEM_RELEASE);
        return false;
    }

    // Ожидаем завершения и освобождаем память
    WaitForSingleObject(hThread.get(), INFINITE);
    VirtualFreeEx(hProcess.get(), pRemotePath, 0, MEM_RELEASE);
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread.get(), &exitCode);
    
    if (exitCode) {
        SwillLogger::Success("DLL injected successfully via LoadLibraryW");
        return true;
    } else {
        SwillLogger::Error("LoadLibraryW returned NULL in target process");
        return false;
    }
}

} // namespace SwillInjector
