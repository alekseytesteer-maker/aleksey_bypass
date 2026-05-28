#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace swill {

// ============================================================================
// Типы данных для хуков
// ============================================================================

struct HookContext {
    void* targetAddress;      // Адрес оригинальной функции
    void* hookFunction;       // Адрес функции-хука
    void* trampolineBuffer;   // Буфер для трамплина (оригинальный код)
    size_t trampolineSize;    // Размер скопированного кода
    bool isActive;            // Статус активности хука
};

// ============================================================================
// HookEngine - Кастомный движок хуков (замена MinHook)
// Реализует inline-хуки с созданием трамплинов
// ============================================================================

class HookEngine {
public:
    static HookEngine& Instance();

    // Инициализация движка
    bool Initialize();
    
    // Создание хука
    bool CreateHook(void* targetAddress, void* hookFunction, void** originalFunction);
    
    // Включение хука
    bool EnableHook(void* targetAddress);
    
    // Отключение хука
    bool DisableHook(void* targetAddress);
    
    // Удаление хука
    bool RemoveHook(void* targetAddress);
    
    // Применение всех хуков
    bool ApplyAllHooks();
    
    // Удаление всех хуков
    bool RemoveAllHooks();

private:
    HookEngine() = default;
    ~HookEngine();
    
    // Копирование инструкций для создания трамплина
    size_t CopyInstructions(void* source, void* destination, size_t minSize);
    
    // Вычисление размера инструкции
    size_t GetInstructionSize(const uint8_t* code);
    
    // Создание JMP инструкции (5 байт: E9 + rel32)
    void WriteJmp(void* source, void* destination);
    
    // Временная защита памяти
    bool ProtectMemory(void* address, size_t size, DWORD newProtect, PDWORD oldProtect);

    std::vector<HookContext> m_hooks;
    bool m_initialized;
};

// ============================================================================
// PatternScanner - Поиск байтовых сигнатур в памяти
// ============================================================================

class PatternScanner {
public:
    // Поиск паттерна в модуле
    void* FindPattern(const char* moduleName, const char* pattern, const char* mask);
    
    // Поиск паттерна в диапазоне адресов
    void* FindPatternInRange(void* start, size_t size, const char* pattern, const char* mask);
    
    // Получение адреса экспортируемой функции
    void* GetExportAddress(HMODULE module, const char* functionName);

private:
    // Сравнение байтов с маской
    bool CompareBytes(const uint8_t* data, const uint8_t* pattern, const char* mask, size_t length);
};

// ============================================================================
// MemoryUtils - Утилиты для работы с памятью
// ============================================================================

class MemoryUtils {
public:
    // Запись данных в память с изменением защиты
    static bool WriteMemory(void* address, const void* data, size_t size);
    
    // Запись NOP-ов
    static bool WriteNop(void* address, size_t count);
    
    // Патч байтов
    static bool PatchBytes(void* address, const std::vector<uint8_t>& bytes);
    
    // Чтение памяти процесса
    static bool ReadProcessMemory(HANDLE hProcess, void* address, void* buffer, size_t size);
    
    // Запись в память процесса
    static bool WriteProcessMemory(HANDLE hProcess, void* address, const void* buffer, size_t size);
    
    // Выделение памяти в процессе
    static void* AllocateMemory(HANDLE hProcess, size_t size);
    
    // Освобождение памяти в процессе
    static bool FreeMemory(HANDLE hProcess, void* address);
};

// ============================================================================
// AntiTamper - Обход системы защиты TamperGuard (netc.dll)
// ============================================================================

class AntiTamper {
public:
    // Отключение флага g_tamper_lock
    static bool DisableTamperLock();
    
    // Патч функции FUN_1026A540 (обработчик нарушений)
    static bool PatchTamperHandler();
    
    // Обход guard-функций (FUN_1026A4B0, FUN_1026A520)
    static bool BypassGuards();
    
    // Поиск и нейтрализация INT 0x29 (__fastfail)
    static bool NeutralizeFastFail();

private:
    // Адреса функций netc.dll (будут найдены через сигнатуры)
    static void* g_tamperLockAddr;
    static void* g_encObj1Addr;
    static void* g_xorKey1Addr;
    static void* g_encObj2Addr;
    static void* g_xorKey2Addr;
    static void* g_expectedThisAddr;
};

} // namespace swill
