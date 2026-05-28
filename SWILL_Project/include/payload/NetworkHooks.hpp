#pragma once

#include "shared/SharedDefs.hpp"

namespace swill {

// ============================================================================
// NetworkHooks - Перехват сетевых функций netc.dll
// Блокировка античит-репортов и модификация пакетов
// ============================================================================

class NetworkHooks {
public:
    // Инициализация сетевых хуков
    static bool Initialize();
    
    // Отключение хуков
    static bool Shutdown();

private:
    // Хук на SendPacket (блокировка пакетов античита)
    static bool __fastcall hkSendPacket(void* ECX, void* EDX, 
                                         unsigned char packetId, 
                                         void* bitStream,
                                         int priority, 
                                         int reliability, 
                                         int ordering);
    
    // Хук на AC_Logger (блокировка логгирования детектов)
    static void __fastcall hkAC_Logger(void* ECX, void* EDX,
                                        int ID,
                                        std::string* text,
                                        int size,
                                        int unk1,
                                        int unk2);
    
    // Хук на AC_Pulsator (эмуляция успешных проверок)
    static int __fastcall hkAC_Pulsator(void* ECX, void* EDX, char a2);

    // Оригинальные функции (трамплины)
    typedef bool (__thiscall* ptrSendPacket)(void*, unsigned char, void*, int, int, int);
    typedef void (__thiscall* ptrAC_Logger)(void*, int, std::string*, int, int, int);
    typedef int (__thiscall* ptrAC_Pulsator)(void*, char);
    
    static ptrSendPacket origSendPacket;
    static ptrAC_Logger origAC_Logger;
    static ptrAC_Pulsator origAC_Pulsator;
    
    // Адреса функций (найдены через сигнатуры)
    static void* addrSendPacket;
    static void* addrAC_Logger;
    static void* addrAC_Pulsator;
};

} // namespace swill
