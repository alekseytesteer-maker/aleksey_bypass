#include "core/HookEngine.hpp"
#include "core/PatternScanner.hpp"
#include "core/MemoryUtils.hpp"
#include "core/AntiTamper.hpp"

// Файлы-заглушки для компиляции
// Реальная реализация находится в заголовочных файлах (inline)

namespace swill {

// Явное определение статических членов AntiTamper
void* AntiTamper::g_tamperLockAddr = nullptr;
void* AntiTamper::g_encObj1Addr = nullptr;
void* AntiTamper::g_xorKey1Addr = nullptr;
void* AntiTamper::g_encObj2Addr = nullptr;
void* AntiTamper::g_xorKey2Addr = nullptr;
void* AntiTamper::g_expectedThisAddr = nullptr;

} // namespace swill
