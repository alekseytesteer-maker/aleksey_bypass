#pragma once
#include <windows.h>
#include <string>
#include <mutex>
#include <fstream>
#include <cstdarg>
#include "../SWILL_Shared/SharedDefs.hpp"

// RAII Guard для HANDLE - предотвращает утечки дескрипторов
struct HandleGuard {
    HANDLE h;
    HandleGuard(HANDLE handle = nullptr) : h(handle) {}
    ~HandleGuard() { 
        if (h && h != INVALID_HANDLE_VALUE) 
            CloseHandle(h); 
    }
    
    // Запрет копирования
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;
    
    // Разрешение перемещения
    HandleGuard(HandleGuard&& other) noexcept : h(other.h) { other.h = nullptr; }
    HandleGuard& operator=(HandleGuard&& other) noexcept {
        if (this != &other) {
            if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
            h = other.h;
            other.h = nullptr;
        }
        return *this;
    }
    
    HANDLE get() const { return h; }
    HANDLE release() { HANDLE tmp = h; h = nullptr; return tmp; }
};

class SwillLogger {
public:
    static bool Initialize(const char* filename);
    static void Shutdown();
    
    static void Log(const char* level, const char* message);
    static void LogF(const char* level, const char* format, ...);
    
    static void Info(const char* message);
    static void Warning(const char* message);
    static void Error(const char* message);
    static void Success(const char* message);
    static void Debug(const char* message);
    
private:
    static std::ofstream s_logFile;
    static std::mutex s_mutex;
    static bool s_initialized;
};
