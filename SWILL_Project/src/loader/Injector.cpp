#include "loader/Injector.hpp"
#include "core/MemoryUtils.hpp"
#include <TlHelp32.h>

namespace swill {

bool Injector::InjectIntoProcess(DWORD processId, const char* dllPath) {
    if (!processId || !dllPath) return false;
    
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        return false;
    }
    
    bool success = InjectViaLoadLibrary(hProcess, dllPath);
    
    CloseHandle(hProcess);
    return success;
}

DWORD Injector::FindGTAProcess() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
    
    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    DWORD gtaPid = 0;
    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (_stricmp(pe32.szExeFile, "gta_sa.exe") == 0) {
                gtaPid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return gtaPid;
}

bool Injector::LaunchAndInject(const char* gtaPath, const char* dllPath) {
    if (!gtaPath || !dllPath) return false;
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    // Запуск GTA в suspended состоянии
    if (!CreateProcessA(gtaPath, nullptr, nullptr, nullptr, FALSE, 
                        CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    
    // Инъекция через SetThreadContext
    bool success = InjectViaSetThreadContext(pi.hProcess, dllPath);
    
    // Возобновление потока
    ResumeThread(pi.hThread);
    
    // Ожидание завершения инъекции
    Sleep(2000);
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    return success;
}

bool Injector::InjectViaLoadLibrary(HANDLE hProcess, const char* dllPath) {
    if (!hProcess || !dllPath) return false;
    
    // Выделение памяти под путь к DLL
    size_t pathLen = strlen(dllPath) + 1;
    void* remoteMem = VirtualAllocEx(hProcess, nullptr, pathLen, 
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) return false;
    
    // Запись пути в память процесса
    if (!WriteProcessMemory(hProcess, remoteMem, dllPath, pathLen, nullptr)) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }
    
    // Получение адреса LoadLibraryA
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLibAddr = GetProcAddress(hKernel32, "LoadLibraryA");
    
    if (!loadLibAddr) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }
    
    // Создание удалённого потока для вызова LoadLibrary
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, 
                                         reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibAddr),
                                         remoteMem, 0, nullptr);
    
    if (!hThread) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }
    
    // Ожидание завершения загрузки
    WaitForSingleObject(hThread, INFINITE);
    
    // Проверка результата
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    
    return (exitCode != 0); // LoadLibrary вернула ненулевой адрес модуля
}

bool Injector::InjectViaSetThreadContext(HANDLE hProcess, const char* dllPath) {
    if (!hProcess || !dllPath) return false;
    
    // Выделение памяти под путь к DLL
    size_t pathLen = strlen(dllPath) + 1;
    void* remoteMem = VirtualAllocEx(hProcess, nullptr, pathLen,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) return false;
    
    // Запись пути
    if (!WriteProcessMemory(hProcess, remoteMem, dllPath, pathLen, nullptr)) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }
    
    // Получение контекста главного потока
    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_FULL;
    
    // Для SetThreadContext нужен хэндл потока с правами
    // В реальном использовании нужно получить хэндл через OpenThread
    // Здесь упрощённая версия
    
    // Получение адреса LoadLibraryA
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLibAddr = GetProcAddress(hKernel32, "LoadLibraryA");
    
    if (!loadLibAddr) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }
    
    // Примечание: полноценная реализация требует получения хэндла потока
    // и модификации EIP через SetThreadContext
    // Это более сложный метод, используемый loader.dll
    
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    
    // Для простоты используем стандартный метод
    return InjectViaLoadLibrary(hProcess, dllPath);
}

} // namespace swill
