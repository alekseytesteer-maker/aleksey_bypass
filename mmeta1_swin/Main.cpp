#include "Injector.hpp"
#include <fstream>
#include <iostream>
#include <string>

int main() {
    setlocale(LC_ALL, "Russian");

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exePathStr(exePath);
    size_t pos = exePathStr.rfind(static_cast<char>(92));
    if (pos == std::string::npos) pos = exePathStr.rfind('/');
    std::string logPath = (pos == std::string::npos ? exePathStr : exePathStr.substr(0, pos)) + "/swill_loader.log";
    std::ofstream log(logPath, std::ios::out | std::ios::trunc);
    auto writeLog = [&](const std::string& s) {
        log << s << std::endl;
        log.flush();
        std::cout << s << std::endl;
    };

    writeLog("=== SWILL LOADER v0.5 | Kernel Init ===");
    writeLog("[*] Log path: " + logPath);

    if (!SwillInjector::SetDebugPrivilege()) {
        writeLog("[!] Warning: Failed to get SeDebugPrivilege.");
    }

    std::wstring targetProcess = L"gta_sa.exe";
    std::string dllName = "SwillPayload.dll";
    
    char currentDir[MAX_PATH];
    GetModuleFileNameA(NULL, currentDir, MAX_PATH);
    std::string currentDirStr(currentDir);
    size_t pos2 = currentDirStr.rfind(static_cast<char>(92));
    if (pos2 == std::string::npos) pos2 = currentDirStr.rfind('/');
    std::string fullDllPath = (pos2 == std::string::npos ? currentDirStr : currentDirStr.substr(0, pos2)) + "/" + dllName;

    writeLog("[*] Looking for gta_sa.exe...");
    writeLog("[*] DLL path: " + fullDllPath);
    DWORD pid = 0;
    int waitCycles = 0;
    while ((pid = SwillInjector::GetProcessId(targetProcess)) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        waitCycles++;
        if (waitCycles > 300) {
            writeLog("[!] Timeout waiting for gta_sa.exe.");
            return 1;
        }
    }

    writeLog("[+] Process found. PID: " + std::to_string(pid));
    writeLog("[*] Injecting: " + fullDllPath);

    if (SwillInjector::InjectDLL(pid, fullDllPath, log)) {
        writeLog("[+++++] SWILL kernel deployed successfully via LoadLibrary!");
    } else {
        writeLog("[!] LoadLibrary injection failed. Trying SetWindowsHookEx...");
        if (SwillInjector::InjectViaHook(pid, fullDllPath, log)) {
            writeLog("[+++++] SWILL kernel deployed successfully via Hook!");
        } else {
            writeLog("[!] Hook injection also failed.");
        }
    }

    writeLog("[+] Done.");
    return 0;
}

