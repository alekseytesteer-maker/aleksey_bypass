#pragma once
#include <windows.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>

class SwillLogger {
private:
    static std::ofstream s_logFile;
    static bool s_initialized;
    static std::string s_logPath;

public:
    // Инициализация логгера
    static void Initialize(const std::string& logPath = "swill_log.txt") {
        if (s_initialized) return;
        
        s_logPath = logPath;
        s_logFile.open(logPath, std::ios::out | std::ios::app);
        
        if (s_logFile.is_open()) {
            s_initialized = true;
            Log("=== SWILL Logger Initialized ===");
            Log("Log file: " + logPath);
        } else {
            std::cerr << "[!] Не удалось открыть файл лога: " << logPath << std::endl;
        }
    }

    // Закрытие логгера
    static void Shutdown() {
        if (!s_initialized) return;
        
        Log("=== SWILL Logger Shutdown ===");
        s_logFile.close();
        s_initialized = false;
    }

    // Логирование строки
    static void Log(const std::string& message) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        std::stringstream ss;
        ss << "[" 
           << std::setfill('0') << std::setw(2) << st.wHour << ":"
           << std::setfill('0') << std::setw(2) << st.wMinute << ":"
           << std::setfill('0') << std::setw(2) << st.wSecond << "."
           << std::setfill('0') << std::setw(3) << st.wMilliseconds
           << "] " << message;
        
        std::string logLine = ss.str();
        
        // Вывод в консоль если она есть
        std::cout << logLine << std::endl;
        
        // Запись в файл
        if (s_initialized && s_logFile.is_open()) {
            s_logFile << logLine << std::endl;
            s_logFile.flush();
        }
    }

    // Логирование с адресом
    static void LogHex(const std::string& message, uintptr_t address) {
        std::stringstream ss;
        ss << message << " 0x" << std::hex << std::uppercase << address << std::dec << std::nouppercase;
        Log(ss.str());
    }

    // Логирование ошибки
    static void LogError(const std::string& error) {
        Log("[ERROR] " + error);
    }

    // Логирование успеха
    static void LogSuccess(const std::string& success) {
        Log("[SUCCESS] " + success);
    }

    // Логирование предупреждения
    static void LogWarning(const std::string& warning) {
        Log("[WARNING] " + warning);
    }

    // Проверка инициализации
    static bool IsInitialized() {
        return s_initialized;
    }
};

// Статические члены класса
inline std::ofstream SwillLogger::s_logFile;
inline bool SwillLogger::s_initialized = false;
inline std::string SwillLogger::s_logPath;

// Макросы для удобного логирования
#define SWILL_LOG(msg) SwillLogger::Log(msg)
#define SWILL_LOG_HEX(msg, addr) SwillLogger::LogHex(msg, addr)
#define SWILL_LOG_ERROR(msg) SwillLogger::LogError(msg)
#define SWILL_LOG_SUCCESS(msg) SwillLogger::LogSuccess(msg)
#define SWILL_LOG_WARNING(msg) SwillLogger::LogWarning(msg)
