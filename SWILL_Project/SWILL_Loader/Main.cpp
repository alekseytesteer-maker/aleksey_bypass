#include "Injector.hpp"
#include <locale>
#include <codecvt>

int main() {
    // Установка русской локали для консоли
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, "Russian");
    
    std::wcout << L"=== SWILL LOADER v1.0 | Инициализация Ядра ===" << std::endl;

    // Попытка получить права отладки
    if (!SwillInjector::SetDebugPrivilege()) {
        std::cout << "[!] Предупреждение: Не удалось получить права SeDebugPrivilege." << std::endl;
        std::cout << "    Инжектор будет работать с ограниченными правами." << std::endl;
    } else {
        std::cout << "[+] Права SeDebugPrivilege успешно получены." << std::endl;
    }

    std::wstring targetProcess = L"gta_sa.exe";
    std::string dllName = "SwillPayload.dll";
    
    // Получаем полный путь к DLL в текущей директории
    char currentDir[MAX_PATH];
    GetModuleFileNameA(NULL, currentDir, MAX_PATH);
    std::string fullPath(currentDir);
    size_t lastSlash = fullPath.find_last_of("\\/");
    std::string currentDirPath = (lastSlash != std::string::npos) ? fullPath.substr(0, lastSlash) : "";
    std::string fullDllPath = currentDirPath + "\\" + dllName;

    std::cout << "[*] Целевой процесс: " << std::string(targetProcess.begin(), targetProcess.end()) << std::endl;
    std::cout << "[*] Путь к DLL: " << fullDllPath << std::endl;
    
    // Проверка существования файла DLL
    if (GetFileAttributesA(fullDllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "[!] КРИТИЧЕСКАЯ ОШИБКА: Файл " << dllName << " не найден в директории инжектора!" << std::endl;
        std::cout << "\nУбедитесь, что SwillPayload.dll находится в той же папке, что и SWILL_Loader.exe" << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "[*] Ожидание запуска процесса gta_sa.exe..." << std::endl;
    DWORD pid = 0;
    int attempts = 0;
    
    while ((pid = SwillInjector::GetProcessId(targetProcess)) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        attempts++;
        
        if (attempts % 4 == 0) {
            std::cout << "." << std::flush;
        }
        
        // Таймаут 60 секунд
        if (attempts > 120) {
            std::cerr << "\n[!] Процесс не найден в течение 60 секунд. Прерывание." << std::endl;
            system("pause");
            return 1;
        }
    }

    std::cout << "\n[+] Процесс обнаружен! PID: " << pid << std::endl;
    std::cout << "[*] Начало внедрения компонента..." << std::endl;

    if (SwillInjector::InjectDLL(pid, fullDllPath)) {
        std::cout << "\n[+++++] УСПЕХ! Сигнатурное ядро SWILL успешно развернуто внутри процесса!" << std::endl;
        std::cout << "[*] Проверьте консоль игры или файл swill_log.txt для деталей." << std::endl;
    } else {
        std::cerr << "\n[!] КРИТИЧЕСКАЯ ОШИБКА при инжекте DLL." << std::endl;
        std::cerr << "    Возможные причины:" << std::endl;
        std::cerr << "    - Антивирус блокирует инъекцию" << std::endl;
        std::cerr << "    - Недостаточно прав доступа" << std::endl;
        std::cerr << "    - Несоответствие архитектуры (x86/x64)" << std::endl;
    }

    std::cout << "\nНажмите любую клавишу для выхода..." << std::endl;
    system("pause");
    return 0;
}
