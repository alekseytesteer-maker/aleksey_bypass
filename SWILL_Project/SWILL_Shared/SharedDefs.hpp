#pragma once
#define SWILL_VERSION "1.0.0"
#define SWILL_VERSION_MAJOR 1
#define SWILL_VERSION_MINOR 0
#define SWILL_VERSION_PATCH 0

// Общие константы для всего проекта
namespace SwillShared {
    // Имена процессов
    constexpr const wchar_t* TARGET_PROCESS_MTA = L"Multi Theft Auto.exe";
    constexpr const wchar_t* TARGET_PROCESS_GTA = L"gta_sa.exe";
    
    // Имена модулей
    constexpr const wchar_t* MODULE_NETC = L"netc.dll";
    
    // IPC Shared Memory имя
    constexpr const char* SHARED_MEMORY_NAME = "Local\\SWILL_SharedBuffer";
    
    // Лог файлы
    constexpr const char* LOG_FILE_PAYLOAD = "swill_payload.log";
    constexpr const char* LOG_FILE_INJECTOR = "swill_injector.log";
    constexpr const char* CRASH_DUMP_FILE = "swill_crash.dmp";
    
    // Сигнатуры (AOB Patterns)
    namespace Signatures {
        // CIdArray::PopUniqueId (FUN_10241990)
        constexpr const char* POP_UNIQUE_ID = "55 8B EC 83 EC ?? 53 56 57 A1 ?? ?? ?? ?? 33 C5 89 45 F4 8B 1D";
        
        // Обработчик пакетов NetBitStream (FUN_100e62b0)
        constexpr const char* NET_PACKET_HANDLER = "55 8B EC 81 EC 0C 04 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 FC 53 56 57 8B F1";
        
        // Ловушка краша (mov [0], 0)
        constexpr const char* CRASH_TRAP = "C7 05 00 00 00 00 00 00 00 00";
        
        // Watson crash handler
        constexpr const char* WATSON_HANDLER = "FF 15 ?? ?? ?? ?? 8B C5 5F 5E 5B 8B E5 5D C3";
        
        // Валидатор стека (FUN_103529d3)
        constexpr const char* STACK_VALIDATOR = "8B 45 ?? 2B 45 ?? 83 C0 FC 3D 1F 00 00 00 77 ?? 8B";
        
        // Lua регистрация функций (FUN_101f4030)
        constexpr const char* LUA_REGISTER = "6A 00 FF 76 04 FF 36 E8 ?? ?? ?? ?? 83 C6 08 83 C4 0C 81 FE ?? ?? ?? ?? 75 E6";
        
        // Epilogue hijacking
        constexpr const char* EPILOGUE_HIJACK = "8B E5 0F B7 EC 56 E9 ?? ?? ?? ??";
        
        // getResourceState (FUN_101f2ca0)
        constexpr const char* RESOURCE_STATE = "55 8B EC 51 53 56 57 8B F9 E8 ?? ?? ?? ?? 8B 1D ?? ?? ?? ?? 8B 75 08";
    }
    
    // Смещения структуры CIdArray (относительно базового адреса 0x105c8b58)
    namespace CIdArrayOffsets {
        constexpr size_t CAPACITY = 0x00;          // m_uiCapacity
        constexpr size_t IS_INITIALIZED = 0x04;    // IsInitialized flag
        constexpr size_t POP_ID_COUNTER = 0x08;    // m_uiPopIdCounter
        constexpr size_t TIMEOUT_LIMIT = 0x0C;     // m_uiTimeoutLimit (3600000)
        constexpr size_t ID_STACK_CAPACITY = 0x10; // m_IDStackCapacity
        constexpr size_t ID_STACK_COUNT = 0x24;    // DAT_105c8b7c - счетчик свободных ID
        constexpr size_t ARRAY_START = 0x28;       // DAT_105c8b80 - указатель на начало массива
        constexpr size_t ARRAY_END = 0x2C;         // DAT_105c8b84 - указатель на конец массива
    }
    
    // Структура IPC команды
    struct IPCCommand {
        enum class Type : uint32_t {
            NONE = 0,
            RELOAD_CONFIG = 1,
            UNHOOK_ALL = 2,
            DUMP_STATE = 3,
            EMERGENCY_SHUTDOWN = 4
        };
        
        Type commandType;
        uint32_t param1;
        uint32_t param2;
        char data[256];
    };
    
    // Структура разделяемой памяти
    struct SharedMemoryData {
        bool isPayloadReady;
        bool isCoreInitialized;
        IPCCommand injectorToPayload;
        IPCCommand payloadToInjector;
        uint32_t crc32; // Для проверки целостности данных
    };
}
