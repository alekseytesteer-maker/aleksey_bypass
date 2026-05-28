cmake_minimum_required(VERSION 3.15)
project(SWILL VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Опции сборки
option(BUILD_LOADER "Build SWILL Loader" ON)
option(BUILD_PAYLOAD "Build SWILL Payload (DLL)" ON)

# Включаем ASM для кастомного хукового движка
enable_language(ASM_MASM)

# ============================================
# SWILL Core Library (Shared)
# ============================================
add_library(SWILL_Core STATIC
    src/core/HookEngine.cpp
    src/core/PatternScanner.cpp
    src/core/MemoryUtils.cpp
    src/core/AntiTamper.cpp
    include/core/HookEngine.hpp
    include/core/PatternScanner.hpp
    include/core/MemoryUtils.hpp
    include/core/AntiTamper.hpp
    include/shared/SharedDefs.hpp
)

target_include_directories(SWILL_Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================
# SWILL Payload (DLL for Injection)
# ============================================
if(BUILD_PAYLOAD)
    add_library(SWILL_Payload SHARED
        src/payload/dllmain.cpp
        src/payload/LuaIntegration.cpp
        src/payload/NetworkHooks.cpp
        src/payload/GuardBypass.cpp
        include/payload/LuaIntegration.hpp
        include/payload/NetworkHooks.hpp
        include/payload/GuardBypass.hpp
    )

    target_link_libraries(SWILL_Payload PRIVATE
        SWILL_Core
    )

    target_compile_definitions(SWILL_Payload PRIVATE
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
    )

    set_target_properties(SWILL_Payload PROPERTIES
        PREFIX ""
        OUTPUT_NAME "SWILL"
    )
endif()

# ============================================
# SWILL Loader (External Injector)
# ============================================
if(BUILD_LOADER)
    add_executable(SWILL_Loader
        src/loader/main.cpp
        src/loader/Injector.cpp
        include/loader/Injector.hpp
    )

    target_link_libraries(SWILL_Loader PRIVATE
        SWILL_Core
        psapi
        dbghelp
    )

    target_compile_definitions(SWILL_Loader PRIVATE
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
    )

    set_target_properties(SWILL_Loader PROPERTIES
        OUTPUT_NAME "SWILL_Loader"
    )
endif()
