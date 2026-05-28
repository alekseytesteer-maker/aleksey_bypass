#include "loader/Injector.hpp"
#include <TlHelp32.h>
#include <chrono>

namespace swill {

bool ProcessUtils::FindProcessByName(const std::wstring& processName, DWORD& outPid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W pe32 = { sizeof(PROCESSENTRY32W) };
    bool found = false;

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                outPid = pe32.th32ProcessID;
                found = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return found;
}

std::vector<DWORD> ProcessUtils::FindAllProcessesByName(const std::wstring& processName) {
    std::vector<DWORD> pids;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return pids;

    PROCESSENTRY32W pe32 = { sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                pids.push_back(pe32.th32ProcessID);
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return pids;
}

bool ProcessUtils::GetModuleBaseAddress(DWORD pid, const std::wstring& moduleName, uintptr_t& baseAddr, SIZE_T& size) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    MODULEENTRY32W me32 = { sizeof(MODULEENTRY32W) };
    bool found = false;

    if (Module32FirstW(hSnapshot, &me32)) {
        do {
            if (_wcsicmp(me32.szModule, moduleName.c_str()) == 0 ||
                _wcsicmp(me32.szExePath, moduleName.c_str()) == 0) {
                baseAddr = reinterpret_cast<uintptr_t>(me32.modBaseAddr);
                size = me32.modBaseSize;
                found = true;
                break;
            }
        } while (Module32NextW(hSnapshot, &me32));
    }

    CloseHandle(hSnapshot);
    return found;
}

bool ProcessUtils::IsProcessRunning(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess)
        return false;

    DWORD exitCode;
    BOOL result = GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);

    return result && exitCode == STILL_ACTIVE;
}

bool ProcessUtils::WaitForProcess(const std::wstring& processName, int timeoutMs, DWORD& outPid) {
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        if (FindProcessByName(processName, outPid))
            return true;

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeoutMs)
            return false;

        Sleep(100);
    }
}

} // namespace swill
