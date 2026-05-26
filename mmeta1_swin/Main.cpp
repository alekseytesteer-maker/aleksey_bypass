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

    writeLog("=== SWILL LOADER v0.7 | Enhanced Kernel Init ===");
    writeLog("[*] Log path: " + logPath);
    writeLog("[!] DISCLAIMER: This tool is for educational purposes only.");
    writeLog("[!] Using this software in online games may result in permanent bans.");
    writeLog("[!] The authors are not responsible for any consequences.");

    // Проверяем, запущен ли процесс от имени администратора
    if (!SwillInjector::IsProcessElevated()) {
        writeLog("[!] WARNING: Loader is NOT running as Administrator.");
        writeLog("[*] Many injection methods require elevated privileges.");
        writeLog("[*] Attempting to restart with admin rights...");
        
        std::wstring wideExePath(exePathStr.begin(), exePathStr.end());
        if (SwillInjector::RunAsAdmin(wideExePath)) {
            writeLog("[->] Restart request sent. Exiting current instance.");
            return 0;
        } else {
            writeLog("[!] Failed to request elevation. Continuing with limited rights (injection may fail).");
            writeLog("[!] HINT: Right-click SwillLoader.exe and select 'Run as Administrator'.");
        }
    } else {
        writeLog("[+] Running with Administrator privileges.");
    }

    if (!SwillInjector::SetDebugPrivilege()) {
        writeLog("[!] Warning: Failed to get SeDebugPrivilege (may require Admin rights).");
    } else {
        writeLog("[->] SeDebugPrivilege enabled.");
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

    // Попытка 1: Классическая инъекция через LoadLibrary
    if (SwillInjector::InjectDLL(pid, fullDllPath, log)) {
        writeLog("[+++++] SWILL kernel deployed successfully via LoadLibrary!");
    } else {
        writeLog("[!] LoadLibrary injection failed. Trying SetWindowsHookEx...");
        // Попытка 2: Инъекция через Windows Hook
        if (SwillInjector::InjectViaHook(pid, fullDllPath, log)) {
            writeLog("[+++++] SWILL kernel deployed successfully via Hook!");
        } else {
            writeLog("[!] Hook injection also failed.");
            writeLog("==================================================");
            writeLog("[!] CRITICAL: All injection methods failed.");
            writeLog("[!] Possible reasons:");
            writeLog("    1. Target process has higher integrity level (run as Admin).");
            writeLog("    2. Antivirus/EDR blocking injection (try disabling temporarily).");
            writeLog("    3. Target process is a Protected Process (PPL).");
            writeLog("    4. Game has anti-cheat protection (kernel-mode driver required).");
            writeLog("    5. Handle stripping via ObRegisterCallbacks is active.");
            writeLog("");
            writeLog("[*] RECOMMENDATIONS FOR BYPASS:");
            writeLog("    - Use a kernel-mode driver to bypass handle restrictions.");
            writeLog("    - Implement manual DLL mapping to avoid LoadLibrary detection.");
            writeLog("    - Use APC injection or thread hijacking for stealth.");
            writeLog("    - Disable page protection changes detection (RWX regions).");
            writeLog("==================================================");
        }
    }

    writeLog("[+] Done.");
    return 0;
}

