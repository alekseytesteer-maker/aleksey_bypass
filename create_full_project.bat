@echo off
setlocal enabledelayedexpansion

echo ================================================================
echo SWILL PROJECT FULL GENERATOR
echo Creating directories...
echo ================================================================

mkdir SWILL_Project
cd SWILL_Project
mkdir 3rdparty
mkdir 3rdparty\MinHook
mkdir 3rdparty\MinHook\include
mkdir 3rdparty\MinHook\lib
mkdir SWILL_Shared
mkdir SWILL_Loader
mkdir SWILL_Payload
mkdir bin
mkdir docs

echo ================================================================
echo Downloading MinHook...
echo ================================================================
powershell -Command "& {Invoke-WebRequest -Uri 'https://github.com/TsudaKageyu/minhook/releases/download/v1.3.3/MinHook-v1.3.3-bin.zip' -OutFile 'minhook.zip'}"
powershell -Command "& {Expand-Archive 'minhook.zip' -DestinationPath 'temp_minhook' -Force}"
copy /Y temp_minhook\MinHook-v1.3.3-bin\Include\* 3rdparty\MinHook\include\
copy /Y temp_minhook\MinHook-v1.3.3-bin\lib\libMinHook.x86.lib 3rdparty\MinHook\lib\MinHook.lib
del minhook.zip
rmdir /S /Q temp_minhook

echo ================================================================
echo Generating Source Files...
echo ================================================================

:: --- SHARED DEFS ---
echo Creating SharedDefs.hpp...
(
echo #pragma once
echo #define SWILL_VERSION "1.0.0"
echo #define IPC_BUFFER_NAME L"Local\\SWILL_SharedBuffer"
echo #define TARGET_PROCESS_L L"gta_sa.exe"
echo #define TARGET_PROCESS_M L"Multi Theft Auto.exe"
echo.
echo struct SharedConfig {
echo     bool gui_enabled;
echo     bool dump_packets;
echo     bool protect_cidarray;
echo     unsigned int debug_level;
echo };
) > SWILL_Shared\SharedDefs.hpp

:: --- INJECTOR HEADERS ---
echo Creating Injector.hpp...
(
echo #pragma once
echo #include ^<windows.h^>
echo #include ^<string^>
echo.
echo class SwillInjector {
echo public:
echo     static bool SetDebugPrivilege();
echo     static DWORD GetProcessId(const std::wstring^& procName);
echo     static BOOL InjectViaTinyCave(DWORD pid, const wchar_t* dllPath);
echo     static BOOL ReflectiveInject(DWORD pid, void* payload, size_t size);
echo private:
echo     static BOOL GrantProcessWriteAccess(DWORD pid);
echo     static DWORD GetSyscallNumber(const char* name);
echo     static void* MakeSyscallStub(DWORD sysno, BYTE argCount);
echo };
) > SWILL_Loader\Injector.hpp

:: --- INJECTOR MAIN (FULL LOGIC) ---
echo Creating Main.cpp (Injector)...
(
echo #define _CRT_SECURE_NO_WARNINGS
echo #include ^<windows.h^>
echo #include ^<tlhelp32.h^>
echo #include ^<psapi.h^>
echo #include ^<aclapi.h^>
echo #include ^<iostream^>
echo #include ^<vector^>
echo #include ^<string^>
echo #include "Injector.hpp"
echo #include "../SWILL_Shared/SharedDefs.hpp"
echo #pragma comment(lib, "psapi.lib")
echo #pragma comment(lib, "advapi32.lib")
echo.
echo extern unsigned char g_payload[];
echo extern unsigned int g_payload_len;
echo.
echo // ... [LOGGING FUNCTIONS] ...
echo void LogMsg(const char* msg) { std::cout << msg << std::endl; }
echo void LogMsgF(const char* fmt, ...) {
echo     char buf[1024]; va_list args; va_start(args, fmt); vsnprintf(buf, sizeof(buf), fmt, args); va_end(args);
echo     LogMsg(buf);
echo }
echo.
echo int main() {
echo     SetConsoleTitleA("SWILL MTA Injector v1.0");
echo     LogMsg("[*] SWILL Injector Started. Waiting for MTA/GTA...");
echo     SwillInjector::SetDebugPrivilege();
echo.
echo     if (g_payload_len ^< 2 || g_payload[0] != 0x4D || g_payload[1] != 0x5A) {
echo         LogMsg("[!] ERROR: Embedded DLL is invalid or missing!");
echo         LogMsg("[!] Please build SWILL_Payload first and copy to swill_payload.bin");
echo         system("pause"); return 1;
echo     }
echo.
echo     DWORD pidGta = 0;
echo     for(int i=0; i^<600; i++) {
echo         Sleep(100);
echo         if(i%50==0) LogMsgF("[*] Scanning... (%d s)", i/10);
echo         pidGta = SwillInjector::GetProcessId(TARGET_PROCESS_L);
echo         if(pidGta) break;
echo     }
echo.
echo     if(!pidGta) { LogMsg("[-] Process not found."); system("pause"); return 1; }
echo.
echo     LogMsgF("[+] Found gta_sa.exe (PID: %d)", pidGta);
echo     LogMsg("[*] Attempting injection via TinyCave...");
echo.
echo     // Путь к временной DLL для инъекции
echo     wchar_t tempPath[MAX_PATH];
echo     GetTempPathW(MAX_PATH, tempPath);
echo     wcscat_s(tempPath, L"swill_temp.dll");
echo     HANDLE hFile = CreateFileW(tempPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
echo     if(hFile != INVALID_HANDLE_VALUE) {
echo         WriteFile(hFile, g_payload, g_payload_len, NULL, NULL);
echo         CloseHandle(hFile);
echo     }
echo.
echo     if(SwillInjector::InjectViaTinyCave(pidGta, tempPath)) {
echo         LogMsg("[+++++] Injection Successful!");
echo         DeleteFileW(tempPath);
echo     } else {
echo         LogMsg("[-] Injection Failed. Trying Reflective...");
echo         if(SwillInjector::ReflectiveInject(pidGta, g_payload, g_payload_len)) {
echo             LogMsg("[+++++] Reflective Injection Successful!");
echo         } else {
echo             LogMsg("[-] All methods failed.");
echo         }
echo     }
echo.
echo     LogMsg("[*] Done. Check game window for GUI (INSERT key).");
echo     system("pause");
echo     return 0;
echo }
) > SWILL_Loader\Main.cpp

:: --- PAYLOAD HEADERS ---
echo Creating Memory.hpp...
(
echo #pragma once
echo #include ^<windows.h^>
echo #include ^<vector^>
echo #include ^<string^>
echo #include ^<sstream^>
echo #include ^<psapi.h^>
echo #pragma comment(lib, "psapi.lib")
echo.
echo class SwillMemory {
echo public:
echo     static bool Patch(void* dst, void* src, size_t size);
echo     static bool Nop(void* dst, size_t size);
echo     static uintptr_t FindPattern(HMODULE hModule, const char* pattern);
echo     template^<typename T^>
echo     static T ReadSafe(uintptr_t addr) {
echo         __try {
echo             MEMORY_BASIC_INFORMATION mbi;
echo             if(VirtualQuery((LPCVOID)addr, ^&mbi, sizeof(mbi)) && (mbi.Protect ^& PAGE_GUARD) == 0) {
echo                 return *(T*)addr;
echo             }
echo         } __except(EXCEPTION_EXECUTE_HANDLER) {}
echo         return T();
echo     }
echo };
) > SWILL_Payload\Memory.hpp

echo Creating Hooks.hpp...
(
echo #pragma once
echo #include "../3rdparty/MinHook/include/MinHook.h"
echo #include ^<iostream^>
echo.
echo class SwillHooks {
echo public:
echo     static bool Initialize();
echo     static bool CreateHook(void* target, void* detour, void** original);
echo     static void Shutdown();
echo };
echo.
echo // Типы функций
echo typedef unsigned int (__fastcall *TPopUniqueId)(void* pThis, void* edx, void* param_1);
echo extern TPopUniqueId g_OriginalPopUniqueId;
echo.
echo typedef void (__stdcall *TPacketHandler)(void* pBitStream, int packetId);
echo extern TPacketHandler g_OriginalPacketHandler;
) > SWILL_Payload\Hooks.hpp

:: --- PAYLOAD CORE (WITH GUI & FIXES) ---
echo Creating Core.cpp...
(
echo #include "Memory.hpp"
echo #include "Hooks.hpp"
echo #include "../SWILL_Shared/SharedDefs.hpp"
echo #include ^<iostream^>
echo #include ^<atomic^>
echo.
echo // Глобальные переменные
echo TPopUniqueId g_OriginalPopUniqueId = nullptr;
echo TPacketHandler g_OriginalPacketHandler = nullptr;
echo std::atomic^<uintptr_t^> g_CIdArray_IsInitialized(0);
echo std::atomic^<uintptr_t^> g_CIdArray_IDStackCount(0);
echo bool g_GUI_Initialized = false;
echo.
echo // Хук PopUniqueId (__fastcall fix)
echo unsigned int __fastcall Hooked_PopUniqueId(void* pThis, void* edx, void* param_1) {
echo     __try {
echo         char isInit = SwillMemory::ReadSafe^<char^>(g_CIdArray_IsInitialized);
echo         if(!isInit) return g_OriginalPopUniqueId(pThis, edx, param_1);
echo.
echo         int stackCount = SwillMemory::ReadSafe^<int^>(g_CIdArray_IDStackCount);
echo         if(stackCount ^<= 0) {
echo             // Защита от краша при пустом стеке
echo             std::cout << "[SWILL] Prevented crash: Empty ID Stack!" << std::endl;
echo             return 0x2000000 + (unsigned int)(pThis); // Fake ID
echo         }
echo     } __except(EXCEPTION_EXECUTE_HANDLER) {
echo         std::cout << "[SWILL] Exception in PopUniqueId hook!" << std::endl;
echo     }
echo.
echo     if(g_OriginalPopUniqueId) return g_OriginalPopUniqueId(pThis, edx, param_1);
echo     return 0;
echo }
echo.
echo // Заглушка для GUI (ImGui требует инициализации DirectX)
echo void RenderGUI() {
echo     if(GetAsyncKeyState(VK_INSERT) ^& 1) g_GUI_Initialized = !g_GUI_Initialized;
echo     if(g_GUI_Initialized) {
echo         // Здесь был бы код ImGui::Begin/End
echo         // В реальном проекте тут рендеринг окна
echo         std::cout << "[GUI] Menu Opened (Placeholder)" << std::endl;
echo     }
echo }
echo.
echo DWORD WINAPI SwillCoreThread(LPVOID lpParam) {
echo     AllocConsole();
echo     freopen("CONOUT$", "w", stdout);
echo     std::cout << "=== SWILL PAYLOAD STARTED ===" << std::endl;
echo.
echo     if(!SwillHooks::Initialize()) return 1;
echo.
echo     HMODULE hNetc = nullptr;
echo     while(!(hNetc = GetModuleHandleW(L"netc.dll"))) Sleep(500);
echo     std::cout << "[+] netc.dll found." << std::endl;
echo.
echo     // Поиск сигнатур
echo     uintptr_t popAddr = SwillMemory::FindPattern(hNetc, "55 8B EC 83 EC ?? 53 56 57 A1 ?? ?? ?? ?? 33 C5 89 45 F4 8B 1D");
echo     if(popAddr) {
echo         std::cout << "[+] Found PopUniqueId: 0x" << std::hex << popAddr << std::endl;
echo         // Извлечение адреса глобальной переменной из инструкции mov eax, [addr]
echo         uintptr_t globalPtr = *(uintptr_t*)(popAddr + 11);
echo         g_CIdArray_IsInitialized = globalPtr + 0x04;
echo         g_CIdArray_IDStackCount = globalPtr + 0x24;
echo.
echo         SwillHooks::CreateHook((void*)popAddr, (void*)Hooked_PopUniqueId, (void**)&g_OriginalPopUniqueId);
echo     }
echo.
echo     std::cout << "[*] Hooks installed. Running..." << std::endl;
echo     while(true) {
echo         RenderGUI();
echo         Sleep(10);
echo     }
echo }
) > SWILL_Payload\Core.cpp

echo Creating dllmain.cpp...
(
echo #include ^<windows.h^>
echo extern DWORD WINAPI SwillCoreThread(LPVOID lpParam);
echo.
echo BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason, LPVOID lpReserved) {
echo     if(ul_reason == DLL_PROCESS_ATTACH) {
echo         DisableThreadLibraryCalls(hModule);
echo         CreateThread(nullptr, 0, SwillCoreThread, hModule, 0, nullptr);
echo     }
echo     return TRUE;
echo }
) > SWILL_Payload\dllmain.cpp

:: --- VISUAL STUDIO PROJECTS ---
echo Creating Solution and Projects...

:: Solution File
(
echo Microsoft Visual Studio Solution File, Format Version 12.00
echo # Visual Studio Version 17
echo VisualStudioVersion = 17.0.31903.59
echo MinimumVisualStudioVersion = 10.0.40219.1
echo Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "SWILL_Loader", "SWILL_Loader\SWILL_Loader.vcxproj", "{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}"
echo EndProject
echo Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "SWILL_Payload", "SWILL_Payload\SWILL_Payload.vcxproj", "{B2C3D4E5-F6A7-8901-BCDE-F12345678901}"
echo EndProject
echo Global
echo 	GlobalSection(SolutionConfigurationPlatforms) = preSolution
echo 		Debug|Win32 = Debug|Win32
echo 		Release|Win32 = Release|Win32
echo 	EndGlobalSection
echo 	GlobalSection(ProjectConfigurationPlatforms) = postSolution
echo 		{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}.Debug|Win32.ActiveCfg = Debug|Win32
echo 		{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}.Release|Win32.ActiveCfg = Release|Win32
echo 		{B2C3D4E5-F6A7-8901-BCDE-F12345678901}.Debug|Win32.ActiveCfg = Debug|Win32
echo 		{B2C3D4E5-F6A7-8901-BCDE-F12345678901}.Release|Win32.ActiveCfg = Release|Win32
echo 	EndGlobalSection
echo EndGlobal
) > SWILL_Project.sln

:: Loader VCXPROJ
(
echo ^<?xml version="1.0" encoding="utf-8"?^>
echo ^<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>
echo   ^<ItemGroup Label="ProjectConfigurations"^>
echo     ^<ProjectConfiguration Include="Debug|Win32"^>^<Configuration^>Debug^</Configuration^>^<Platform^>Win32^</Platform^>^</ProjectConfiguration^>
echo     ^<ProjectConfiguration Include="Release|Win32"^>^<Configuration^>Release^</Configuration^>^<Platform^>Win32^</Platform^>^</ProjectConfiguration^>
echo   ^</ItemGroup^>
echo   ^<PropertyGroup Label="Globals"^>
echo     ^<VCProjectVersion^>17.0^</VCProjectVersion^>
echo     ^<ProjectGuid^>{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}^</ProjectGuid^>
echo     ^<RootNamespace^>SWILL_Loader^</RootNamespace^>
echo     ^<WindowsTargetPlatformVersion^>10.0^</WindowsTargetPlatformVersion^>
echo   ^</PropertyGroup^>
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" /^>
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'" Label="Configuration"^>
echo     ^<ConfigurationType^>Application^</ConfigurationType^>
echo     ^<UseDebugLibraries^>false^</UseDebugLibraries^>
echo     ^<PlatformToolset^>v143^</PlatformToolset^>
echo     ^<CharacterSet^>MultiByte^</CharacterSet^>
echo   ^</PropertyGroup^>
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" /^>
echo   ^<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'"^>
echo     ^<ClCompile^>
echo       ^<AdditionalIncludeDirectories^>..\\3rdparty\\MinHook\\include;..\\SWILL_Shared;%(AdditionalIncludeDirectories)^</AdditionalIncludeDirectories^>
echo       ^<PreprocessorDefinitions^>_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)^</PreprocessorDefinitions^>
echo     ^</ClCompile^>
echo     ^<Link^>
echo       ^<AdditionalDependencies^>kernel32.lib;user32.lib;psapi.lib;advapi32.lib;%(AdditionalDependencies)^</AdditionalDependencies^>
echo     ^</Link^>
echo   ^</ItemDefinitionGroup^>
echo   ^<ItemGroup^>
echo     ^<ClCompile Include="Main.cpp" /^>
echo     ^<ClCompile Include="Injector.cpp" /^>
echo   ^</ItemGroup^>
echo   ^<ItemGroup^>
echo     ^<ClInclude Include="Injector.hpp" /^>
echo     ^<ClInclude Include="..\\SWILL_Shared\\SharedDefs.hpp" /^>
echo   ^</ItemGroup^>
echo   ^<ItemGroup^>
echo     ^<None Include="..\\bin\\SwillPayload.dll"^>^<FileType^>Document^</FileType^>^</None^>
echo   ^</ItemGroup^>
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" /^>
echo ^</Project^>
) > SWILL_Loader\SWILL_Loader.vcxproj

:: Payload VCXPROJ
(
echo ^<?xml version="1.0" encoding="utf-8"?^>
echo ^<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>
echo   ^<ItemGroup Label="ProjectConfigurations"^>
echo     ^<ProjectConfiguration Include="Debug|Win32"^>^<Configuration^>Debug^</Configuration^>^<Platform^>Win32^</Platform^>^</ProjectConfiguration^>
echo     ^<ProjectConfiguration Include="Release|Win32"^>^<Configuration^>Release^</Configuration^>^<Platform^>Win32^</Platform^>^</ProjectConfiguration^>
echo   ^</ItemGroup^>
echo   ^<PropertyGroup Label="Globals"^>
echo     ^<VCProjectVersion^>17.0^</VCProjectVersion^>
echo     ^<ProjectGuid^>{B2C3D4E5-F6A7-8901-BCDE-F12345678901}^</ProjectGuid^>
echo     ^<RootNamespace^>SWILL_Payload^</RootNamespace^>
echo     ^<WindowsTargetPlatformVersion^>10.0^</WindowsTargetPlatformVersion^>
echo   ^</PropertyGroup^>
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" /^>
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'" Label="Configuration"^>
echo     ^<ConfigurationType^>DynamicLibrary^</ConfigurationType^>
echo     ^<UseDebugLibraries^>false^</UseDebugLibraries^>
echo     ^<PlatformToolset^>v143^</PlatformToolset^>
echo     ^<CharacterSet^>MultiByte^</CharacterSet^>
echo   ^</PropertyGroup^>
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" /^>
echo   ^<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'"^>
echo     ^<ClCompile^>
echo       ^<AdditionalIncludeDirectories^>..\\3rdparty\\MinHook\\include;..\\SWILL_Shared;%(AdditionalIncludeDirectories)^</AdditionalIncludeDirectories^>
echo       ^<PreprocessorDefinitions^>_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)^</PreprocessorDefinitions^>
echo     ^</ClCompile^>
echo     ^<Link^>
echo       ^<AdditionalDependencies^>kernel32.lib;user32.lib;psapi.lib;..\\3rdparty\\MinHook\\lib\\MinHook.lib;%(AdditionalDependencies)^</AdditionalDependencies^>
echo     ^</Link^>
echo   ^</ItemDefinitionGroup^>
echo   ^<ItemGroup^>
echo     ^<ClCompile Include="dllmain.cpp" /^>
echo     ^<ClCompile Include="Core.cpp" /^>
echo     ^<ClCompile Include="Memory.cpp" /^>
echo     ^<ClCompile Include="Hooks.cpp" /^>
echo   ^</ItemGroup^>
echo   ^<ItemGroup^>
echo     ^<ClInclude Include="Memory.hpp" /^>
echo     ^<ClInclude Include="Hooks.hpp" /^>
echo   ^</ItemGroup^>
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" /^>
echo ^</Project^>
) > SWILL_Payload\SWILL_Payload.vcxproj

:: Missing CPP implementations
echo Creating Injector.cpp...
(
echo #include "Injector.hpp"
echo #include ^<iostream^>
echo // Реализация методов инжектора (сокращенная версия для примера)
echo bool SwillInjector::SetDebugPrivilege() {
echo     HANDLE hToken;
echo     TOKEN_PRIVILEGES tp;
echo     LUID luid;
echo     if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, ^&hToken)) return false;
echo     if(!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, ^&luid)) { CloseHandle(hToken); return false; }
echo     tp.PrivilegeCount = 1;
echo     tp.Privileges[0].Luid = luid;
echo     tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
echo     bool res = AdjustTokenPrivileges(hToken, FALSE, ^&tp, sizeof(tp), NULL, NULL);
echo     CloseHandle(hToken);
echo     return res ^&^& (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
echo }
echo DWORD SwillInjector::GetProcessId(const std::wstring^& name) {
echo     DWORD pid = 0;
echo     HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
echo     if(hSnap != INVALID_HANDLE_VALUE) {
echo         PROCESSENTRY32W pe = {sizeof(pe)};
echo         if(Process32FirstW(hSnap, ^&pe)) {
echo             do { if(_wcsicmp(pe.szExeFile, name.c_str()) == 0) { pid = pe.th32ProcessID; break; } }
echo             while(Process32NextW(hSnap, ^&pe));
echo         }
echo         CloseHandle(hSnap);
echo     }
echo     return pid;
echo }
echo BOOL SwillInjector::InjectViaTinyCave(DWORD pid, const wchar_t* dllPath) {
echo     HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
echo     if(!hProc) return FALSE;
echo     SIZE_T len = (wcslen(dllPath)+1)*sizeof(wchar_t);
echo     LPVOID pRemote = VirtualAllocEx(hProc, NULL, len, MEM_COMMIT, PAGE_READWRITE);
echo     if(!pRemote) { CloseHandle(hProc); return FALSE; }
echo     WriteProcessMemory(hProc, pRemote, dllPath, len, NULL);
echo     FARPROC pLoad = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
echo     HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)pLoad, pRemote, 0, NULL);
echo     if(hThread) { WaitForSingleObject(hThread, 5000); CloseHandle(hThread); }
echo     VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
echo     CloseHandle(hProc);
echo     return (hThread != NULL);
echo }
echo BOOL SwillInjector::ReflectiveInject(DWORD pid, void* payload, size_t size) {
echo     // Упрощенная заглушка
echo     return FALSE;
echo }
echo BOOL SwillInjector::GrantProcessWriteAccess(DWORD pid) { return TRUE; }
echo DWORD SwillInjector::GetSyscallNumber(const char* name) { return 0; }
echo void* SwillInjector::MakeSyscallStub(DWORD sysno, BYTE argCount) { return NULL; }
) > SWILL_Loader\Injector.cpp

echo Creating Memory.cpp...
(
echo #include "Memory.hpp"
echo bool SwillMemory::Patch(void* dst, void* src, size_t size) {
echo     DWORD old;
echo     if(!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, ^&old)) return false;
echo     memcpy(dst, src, size);
echo     VirtualProtect(dst, size, old, ^&old);
echo     return true;
echo }
echo bool SwillMemory::Nop(void* dst, size_t size) {
echo     DWORD old;
echo     if(!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, ^&old)) return false;
echo     memset(dst, 0x90, size);
echo     VirtualProtect(dst, size, old, ^&old);
echo     return true;
echo }
echo uintptr_t SwillMemory::FindPattern(HMODULE hMod, const char* pattern) {
echo     if(!hMod) return 0;
echo     MODULEINFO mi;
echo     GetModuleInformation(GetCurrentProcess(), hMod, ^&mi, sizeof(mi));
echo     BYTE* base = (BYTE*)mi.lpBaseOfDll;
echo     BYTE* end = base + mi.SizeOfImage;
echo     std::vector^<BYTE^> bytes;
echo     std::string mask;
echo     std::stringstream ss(pattern);
echo     std::string token;
echo     while(ss ^^>> token) {
echo         if(token == "??" || token == "?") { bytes.push_back(0); mask += '?'; }
echo         else { bytes.push_back((BYTE)stoul(token, nullptr, 16)); mask += 'x'; }
echo     }
echo     for(BYTE* i = base; i ^< end - bytes.size(); i++) {
echo         bool found = true;
echo         for(size_t j=0; j^<bytes.size(); j++) {
echo             if(mask[j] == 'x' ^&^& i[j] != bytes[j]) { found = false; break; }
echo         }
echo         if(found) return (uintptr_t)i;
echo     }
echo     return 0;
echo }
) > SWILL_Payload\Memory.cpp

echo Creating Hooks.cpp...
(
echo #include "Hooks.hpp"
echo bool SwillHooks::Initialize() {
echo     return MH_Initialize() == MH_OK;
echo }
echo bool SwillHooks::CreateHook(void* target, void* detour, void** original) {
echo     if(MH_CreateHook(target, detour, original) != MH_OK) return false;
echo     return MH_EnableHook(target) == MH_OK;
echo }
echo void SwillHooks::Shutdown() {
echo     MH_DisableHook(MH_ALL_HOOKS);
echo     MH_Uninitialize();
echo }
) > SWILL_Payload\Hooks.cpp

echo ================================================================
echo DONE! Project created in SWILL_Project folder.
echo Next steps:
echo 1. Open SWILL_Project.sln in Visual Studio.
echo 2. Build SWILL_Payload (Release, Win32).
echo 3. Copy bin/SwillPayload.dll to SWILL_Loader/swill_payload.bin (or embed via resource).
echo 4. Build SWILL_Loader.
echo ================================================================
pause
