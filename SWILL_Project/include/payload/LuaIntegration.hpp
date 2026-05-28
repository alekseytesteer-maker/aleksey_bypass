#pragma once

#include "shared/SharedDefs.hpp"

namespace swill {

// ============================================================================
// LuaIntegration - Интеграция с Lua VM MTA:SA
// Версия Lua: 5.1 (статически слинкована в client.dll)
// ============================================================================

class LuaIntegration {
public:
    // Инициализация интеграции с Lua
    static bool Initialize();
    
    // Выполнение Lua кода через loadstring
    static bool ExecuteLua(const char* code, const char* chunkName = "@swill_injected");
    
    // Выполнение Lua кода через luaL_loadbuffer
    static bool ExecuteLuaBuffer(const char* code, size_t length, const char* chunkName);
    
    // Получение lua_State* из текущего контекста
    static void* GetLuaState();
    
    // Регистрация глобальной переменной в Lua
    static bool SetGlobal(const char* name, void* value);
    
    // Вызов Lua функции
    static bool CallLuaFunction(const char* functionName, int args = 0, int results = 0);

private:
    // Адреса функций Lua (будут найдены через сигнатуры)
    static void* g_luaState;
    static void* p_lua_getglobal;
    static void* p_lua_pushstring;
    static void* p_lua_pcall;
    static void* p_luaL_loadstring;
    static void* p_luaL_loadbuffer;
    static void* p_lua_tolstring;
    static void* p_lua_setglobal;
    
    // Поиск lua_State через cross-reference на resourceRoot
    static void* FindLuaState();
};

} // namespace swill
