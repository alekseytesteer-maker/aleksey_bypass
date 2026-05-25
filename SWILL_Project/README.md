# SWILL Project - Advanced MTA Injection Framework

## Overview
SWILL (Stealth Weapon for Injecting Libraries and Loading) is a comprehensive injection framework designed for Multi Theft Auto. It features advanced memory manipulation, signature scanning, and MinHook-based function interception to bypass anti-cheat protections.

## Components

### 1. SWILL_Loader (Injector)
- **SeDebugPrivilege** elevation for system-wide access
- **DACL manipulation** to grant PROCESS_ALL_ACCESS
- **Syscall stubbing** for stealthy memory operations
- **Multiple injection methods**:
  - LoadLibraryW via remote thread
  - Reflective DLL injection (manual mapping)
  - Cave hunting for safe memory allocation
- **Process monitoring** with crash detection

### 2. SWILL_Payload (DLL)
- **MinHook integration** for function interception
- **AOB Pattern Scanner** for dynamic address resolution
- **CIdArray::PopUniqueId hook** to prevent ID allocation crashes
- **Crash trap neutralization** (NOP'd dangerous instructions)
- **NetBitStream handler detection** for future packet interception

## Architecture

```
SWILL_Project/
├── SWILL_Loader/
│   ├── Injector.hpp      # Core injection logic
│   └── Main.cpp          # Entry point
├── SWILL_Payload/
│   ├── Memory.hpp        # AOB scanner & patch engine
│   ├── Hooks.hpp         # MinHook wrapper
│   ├── Core.cpp          # CIdArray hook implementation
│   ├── dllmain.cpp       # DLL entry point
│   └── MinHook/          # MinHook library files
├── bin/                  # Compiled binaries
└── docs/                 # Documentation
```

## Key Features

### CIdArray Protection
Based on decompiled analysis of `FUN_10241990`:
- Monitors `DAT_105c8b5c` (IsInitialized flag)
- Checks `DAT_105c8b7c` (ID stack count)
- Generates virtual IDs when stack is empty
- Prevents crash from `mov [0], 0` trap

### Signature Database
All patterns from technical analysis:
- `55 8B EC 83 EC ?? 53 56 57 A1...` - PopUniqueId
- `55 8B EC 81 EC 0C 04 00 00 A1...` - NetBitStream handler
- `C7 05 00 00 00 00 00 00 00 00` - Crash trap

## Build Instructions

1. **Download MinHook**: Place in `SWILL_Payload/MinHook/`
2. **Open Solution**: `SWILL_Project.sln` in Visual Studio
3. **Configure**: Release | Win32 platform
4. **Build**: Build Solution (Ctrl+Shift+B)
5. **Run**: Execute `SWILL_Loader.exe` as Administrator

## Usage

1. Start MTA San Andreas manually
2. Run `SWILL_Loader.exe` as Administrator
3. Wait for automatic injection (up to 60 seconds)
4. Check `swill_injector.log` on Desktop for status

## Technical Details

### Memory Map (CIdArray)
- `0x105c8b58` - m_uiCapacity
- `0x105c8b5c` - IsInitialized
- `0x105c8b60` - m_uiPopIdCounter
- `0x105c8b64` - m_uiTimeoutLimit (3600000ms)
- `0x105c8b7c` - ID stack count
- `0x105c8b80` - Array start pointer
- `0x105c8b84` - Array end pointer

### Hook Flow
1. Wait for netc.dll module load
2. Scan for PopUniqueId signature
3. Extract CIdArray base from function prologue
4. Install detour hook via MinHook
5. Monitor stack count, generate virtual IDs if needed
6. Neutralize crash traps with NOP patches

## Disclaimer
This project is for educational purposes only. Use responsibly and only on servers where you have permission.
