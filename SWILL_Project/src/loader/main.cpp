// SWILL Loader - Внешний инжектор для загрузки SWILL в процесс GTA:SA
// Альтернатива внутренней инъекции через loader.dll

#include "shared/SharedDefs.hpp"
#include "loader/Injector.hpp"
#include "core/AntiTamper.hpp"

#include <Windows.h>
#include <iostream>
#include <string>

void PrintBanner() {
    std::cout << "========================================" << std::endl;
    std::cout << "   SWILL Loader v1.0.0" << std::endl;
    std::cout << "   MTA:SA Anti-Cheat Bypass" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
}

void PrintUsage(const char* programName) {
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << programName << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -i, --inject     Inject into running gta_sa.exe process" << std::endl;
    std::cout << "  -l, --launch     Launch GTA:SA and inject SWILL" << std::endl;
    std::cout << "  -p, --path PATH  Path to gta_sa.exe (default: auto-detect)" << std::endl;
    std::cout << "  -d, --dll PATH   Path to SWILL.dll (default: current directory)" << std::endl;
    std::cout << "  -h, --help       Show this help message" << std::endl;
    std::cout << std::endl;
}

std::string GetDefaultGTAPath() {
    // Попытка авто-определения пути к GTA из реестра
    HKEY hKey;
    char path[MAX_PATH] = { 0 };
    DWORD size = sizeof(path);
    
    // Rockstar Games Launcher
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
        "SOFTWARE\\Rockstar Games\\Grand Theft Auto San Andreas", 
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "InstallFolder", nullptr, nullptr, 
                         reinterpret_cast<LPBYTE>(path), &size);
        RegCloseKey(hKey);
        
        if (path[0] != '\0') {
            return std::string(path) + "\\gta_sa.exe";
        }
    }
    
    // Steam версия
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Wow6432Node\\Valve\\Steam\\Apps\\12120",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        // Steam требует дополнительного запроса к Steam API
        // Для простоты возвращаем стандартный путь
    }
    
    // Стандартные пути
    const char* defaultPaths[] = {
        "C:\\Program Files (x86)\\Rockstar Games\\GTA San Andreas\\gta_sa.exe",
        "C:\\Program Files\\Rockstar Games\\GTA San Andreas\\gta_sa.exe",
        "C:\\Games\\GTA San Andreas\\gta_sa.exe"
    };
    
    for (const char* p : defaultPaths) {
        if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) {
            return p;
        }
    }
    
    return "";
}

std::string GetCurrentDirectoryPath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    std::string path(buffer);
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return ".";
}

int main(int argc, char* argv[]) {
    PrintBanner();
    
    std::string gtaPath;
    std::string dllPath;
    bool injectMode = false;
    bool launchMode = false;
    
    // Парсинг аргументов
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg == "-i" || arg == "--inject") {
            injectMode = true;
        }
        else if (arg == "-l" || arg == "--launch") {
            launchMode = true;
        }
        else if (arg == "-p" || arg == "--path") {
            if (i + 1 < argc) {
                gtaPath = argv[++i];
            }
        }
        else if (arg == "-d" || arg == "--dll") {
            if (i + 1 < argc) {
                dllPath = argv[++i];
            }
        }
    }
    
    // Установка путей по умолчанию
    if (dllPath.empty()) {
        dllPath = GetCurrentDirectoryPath() + "\\SWILL.dll";
    }
    
    if (gtaPath.empty() && launchMode) {
        gtaPath = GetDefaultGTAPath();
        if (gtaPath.empty()) {
            std::cerr << "Error: Could not auto-detect GTA:SA path." << std::endl;
            std::cerr << "Please specify path with -p option." << std::endl;
            return 1;
        }
    }
    
    // Проверка существования DLL
    if (GetFileAttributesA(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "Error: SWILL.dll not found at: " << dllPath << std::endl;
        return 1;
    }
    
    std::cout << "[*] SWILL DLL: " << dllPath << std::endl;
    
    // Выбор режима работы
    if (injectMode) {
        std::cout << "[*] Searching for gta_sa.exe process..." << std::endl;
        
        DWORD pid = swill::Injector::FindGTAProcess();
        if (pid == 0) {
            std::cerr << "Error: gta_sa.exe process not found." << std::endl;
            std::cerr << "Please start GTA:SA first or use --launch mode." << std::endl;
            return 1;
        }
        
        std::cout << "[*] Found gta_sa.exe (PID: " << pid << ")" << std::endl;
        std::cout << "[*] Injecting SWILL..." << std::endl;
        
        if (swill::Injector::InjectIntoProcess(pid, dllPath.c_str())) {
            std::cout << "[+] Injection successful!" << std::endl;
            return 0;
        } else {
            std::cerr << "[-] Injection failed!" << std::endl;
            return 1;
        }
    }
    else if (launchMode) {
        std::cout << "[*] GTA Path: " << gtaPath << std::endl;
        
        if (GetFileAttributesA(gtaPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            std::cerr << "Error: gta_sa.exe not found at: " << gtaPath << std::endl;
            return 1;
        }
        
        std::cout << "[*] Launching GTA:SA with SWILL injection..." << std::endl;
        
        if (swill::Injector::LaunchAndInject(gtaPath.c_str(), dllPath.c_str())) {
            std::cout << "[+] Launch and injection successful!" << std::endl;
            return 0;
        } else {
            std::cerr << "[-] Launch/injection failed!" << std::endl;
            return 1;
        }
    }
    else {
        std::cout << "No mode specified. Use --inject or --launch." << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }
    
    return 0;
}
