#include "payload/LuaIntegration.hpp"
#include "core/PatternScanner.hpp"
#include "core/MemoryUtils.hpp"

namespace swill {

// Статические члены LuaIntegration
void* LuaIntegration::g_luaState = nullptr;
void* LuaIntegration::p_lua_getglobal = nullptr;
void* LuaIntegration::p_lua_pushstring = nullptr;
void* LuaIntegration::p_lua_pcall = nullptr;
void* LuaIntegration::p_luaL_loadstring = nullptr;
void* LuaIntegration::p_luaL_loadbuffer = nullptr;
void* LuaIntegration::p_lua_tolstring = nullptr;
void* LuaIntegration::p_lua_setglobal = nullptr;

bool LuaIntegration::Initialize() {
    // Поиск client.dll где находится Lua VM
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hClient) {
        hClient = GetModuleHandleA("mods\\deathmatch\\client.dll");
    }
    
    if (!hClient) return false;
    
    PatternScanner scanner;
    
    // Поиск lua_State через строку "resourceRoot" (0x1042a664 в анализе)
    g_luaState = FindLuaState();
    if (!g_luaState) return false;
    
    // Получение адресов функций Lua
    p_lua_getglobal = scanner.GetExportAddress(hClient, "lua_getglobal");
    p_lua_pushstring = scanner.GetExportAddress(hClient, "lua_pushstring");
    p_lua_pcall = scanner.GetExportAddress(hClient, "lua_pcall");
    p_luaL_loadstring = scanner.GetExportAddress(hClient, "luaL_loadstring");
    p_luaL_loadbuffer = scanner.GetExportAddress(hClient, "luaL_loadbuffer");
    p_lua_tolstring = scanner.GetExportAddress(hClient, "lua_tolstring");
    p_lua_setglobal = scanner.GetExportAddress(hClient, "lua_setglobal");
    
    return (p_lua_getglobal && p_lua_pushstring && p_lua_pcall);
}

bool LuaIntegration::ExecuteLua(const char* code, const char* chunkName) {
    if (!g_luaState || !code) return false;
    
    typedef int (__cdecl *luaL_loadstring_t)(void* L, const char* str);
    typedef int (__cdecl *lua_pcall_t)(void* L, int nargs, int nresults, int errfunc);
    
    auto luaL_loadstring = reinterpret_cast<luaL_loadstring_t>(p_luaL_loadstring);
    auto lua_pcall = reinterpret_cast<lua_pcall_t>(p_lua_pcall);
    
    if (!luaL_loadstring || !lua_pcall) return false;
    
    // Загрузка строки как Lua кода
    int loadResult = luaL_loadstring(static_cast<void*>(g_luaState), code);
    if (loadResult != 0) {
        return false; // Ошибка парсинга
    }
    
    // Выполнение с защитой через pcall
    int callResult = lua_pcall(static_cast<void*>(g_luaState), 0, 0, 0);
    return (callResult == 0);
}

bool LuaIntegration::ExecuteLuaBuffer(const char* code, size_t length, const char* chunkName) {
    if (!g_luaState || !code) return false;
    
    typedef int (__cdecl *luaL_loadbuffer_t)(void* L, const char* buff, size_t sz, const char* name);
    typedef int (__cdecl *lua_pcall_t)(void* L, int nargs, int nresults, int errfunc);
    
    auto luaL_loadbuffer = reinterpret_cast<luaL_loadbuffer_t>(p_luaL_loadbuffer);
    auto lua_pcall = reinterpret_cast<lua_pcall_t>(p_lua_pcall);
    
    if (!luaL_loadbuffer || !lua_pcall) return false;
    
    int loadResult = luaL_loadbuffer(static_cast<void*>(g_luaState), code, length, chunkName);
    if (loadResult != 0) return false;
    
    int callResult = lua_pcall(static_cast<void*>(g_luaState), 0, 0, 0);
    return (callResult == 0);
}

void* LuaIntegration::GetLuaState() {
    return g_luaState;
}

bool LuaIntegration::SetGlobal(const char* name, void* value) {
    if (!g_luaState || !name || !p_lua_setglobal) return false;
    
    typedef void (__cdecl *lua_setglobal_t)(void* L, const char* name);
    auto lua_setglobal = reinterpret_cast<lua_setglobal_t>(p_lua_setglobal);
    
    // Предварительно нужно положить значение на стек
    // Эта функция предполагает что значение уже на стеке
    lua_setglobal(static_cast<void*>(g_luaState), name);
    return true;
}

bool LuaIntegration::CallLuaFunction(const char* functionName, int args, int results) {
    if (!g_luaState || !functionName || !p_lua_getglobal || !p_lua_pcall) return false;
    
    typedef void (__cdecl *lua_getglobal_t)(void* L, const char* name);
    typedef int (__cdecl *lua_pcall_t)(void* L, int nargs, int nresults, int errfunc);
    
    auto lua_getglobal = reinterpret_cast<lua_getglobal_t>(p_lua_getglobal);
    auto lua_pcall = reinterpret_cast<lua_pcall_t>(p_lua_pcall);
    
    lua_getglobal(static_cast<void*>(g_luaState), functionName);
    int callResult = lua_pcall(static_cast<void*>(g_luaState), args, results, 0);
    
    return (callResult == 0);
}

void* LuaIntegration::FindLuaState() {
    // Поиск через cross-reference на строку "resourceRoot"
    // В анализе: PUSH 0x1042a664 ("resourceRoot") @ ~0x1013e360
    
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hClient) hClient = GetModuleHandleA("mods\\deathmatch\\client.dll");
    if (!hClient) return nullptr;
    
    PatternScanner scanner;
    
    // Сигнатура: PUSH "resourceRoot" (68 64 A6 42 10)
    const char* sig = "\x68\x64\xA6\x42\x10";
    const char* mask = "xxxxx";
    
    void* addr = scanner.FindPattern("client.dll", sig, mask);
    if (!addr) {
        // Пробуем альтернативный паттерн
        const char* sig2 = "resourceRoot";
        addr = scanner.FindPattern("client.dll", sig2, "xxxxxxxxxxxx");
        if (addr) {
            // Возвращаем саму строку как маркер, lua_State будет рядом
            return addr;
        }
        return nullptr;
    }
    
    // lua_State обычно передаётся как параметр или находится в регистре ECX
    // Для простоты возвращаем найденный адрес как базу для поиска
    return addr;
}

} // namespace swill
