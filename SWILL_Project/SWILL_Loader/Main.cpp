#include "Injector.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")

// Ресурс DLL (будет внедрен через Resource.rc)
extern unsigned char g_payload[];
extern unsigned int g_payload_len;

void WritePayloadToTemp(wchar_t* outPath, size_t maxLen) {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    
    // Удаляем trailing backslash
    size_t len = wcslen(tempPath);
    if (len > 0 && tempPath[len - 1] == L'\\') {
        tempPath[len - 1] = L'\0';
    }
    
    swprintf_s(outPath, maxLen, L"%s\\swill_payload_%d.dll", tempPath, GetCurrentProcessId());
    
    HANDLE hFile = CreateFileW(outPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, g_payload, g_payload_len, &written, NULL);
        CloseHandle(hFile);
        SwillLogger::LogF("INFO", "Payload written to: %ls", outPath);
    } else {
        SwillLogger::Error("Failed to write payload to temp");
    }
}

int main() {
    SetConsoleTitleA("SWILL Injector v" SWILL_VERSION);
    
    // Инициализация логгера инжектора
    SwillLogger::Initialize(SwillShared::LOG_FILE_INJECTOR);
    SwillLogger::Info("=== SWILL INJECTOR STARTED ===");
    SwillLogger::LogF("INFO", "Version: %s", SWILL_VERSION);
    
    // Включаем привилегии
    if (!SwillInjector::SetDebugPrivilege()) {
        SwillLogger::Warning("Failed to enable SeDebugPrivilege, continuing anyway...");
    }
    
    // Проверяем наличие встроенной DLL
    if (g_payload_len < 64 || g_payload[0] != 'M' || g_payload[1] != 'Z') {
        SwillLogger::Error("No valid PE DLL embedded in executable!");
        std::cout << "[!] CRITICAL: No DLL resource found. Please rebuild with payload." << std::endl;
        std::cout << "[i] Build SWILL_Payload first, then copy it to bin/swill_payload.bin" << std::endl;
        system("pause");
        return 1;
    }
    
    SwillLogger::LogF("INFO", "Embedded payload size: %d bytes", g_payload_len);
    std::cout << "[*] Payload loaded: " << g_payload_len << " bytes" << std::endl;
    
    // Ждем процесс GTA
    std::wcout << L"[*] Waiting for " << SwillShared::TARGET_PROCESS_GTA << L"..." << std::endl;
    SwillLogger::LogF("INFO", "Waiting for process: %ls", SwillShared::TARGET_PROCESS_GTA);
    
    DWORD pid = 0;
    for (int i = 0; i < 300; i++) { // 30 секунд
        pid = SwillInjector::GetProcessIdByName(SwillShared::TARGET_PROCESS_GTA);
        if (pid != 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (i % 50 == 0) {
            std::cout << "." << std::flush;
        }
    }
    
    if (pid == 0) {
        std::cout << "\n[!] Process not found after 30 seconds." << std::endl;
        SwillLogger::Error("Target process not found");
        SwillLogger::Shutdown();
        system("pause");
        return 1;
    }
    
    std::wcout << L"\n[+] Found " << SwillShared::TARGET_PROCESS_GTA << L" (PID: " << pid << L")" << std::endl;
    SwillLogger::LogF("SUCCESS", "Target process found: PID %d", pid);
    
    // Пишем DLL во временный файл
    wchar_t dllPath[MAX_PATH];
    WritePayloadToTemp(dllPath, MAX_PATH);
    
    // Инжектируем через LoadLibrary
    std::cout << "[*] Injecting via LoadLibraryW..." << std::endl;
    SwillLogger::Info("Attempting injection via LoadLibraryW");
    
    if (SwillInjector::InjectDLL_LoadLibrary(pid, dllPath)) {
        std::cout << "[+++++] INJECTION SUCCESSFUL!" << std::endl;
        SwillLogger::Success("Injection completed successfully");
    } else {
        std::cout << "[!] Injection failed. Check logs." << std::endl;
        SwillLogger::Error("Injection failed");
    }
    
    std::cout << "\n[*] Check swill_injector.log and swill_payload.log for details." << std::endl;
    system("pause");
    
    SwillLogger::Shutdown();
    return 0;
}
