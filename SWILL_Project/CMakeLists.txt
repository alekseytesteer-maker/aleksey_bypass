cmake_minimum_required(VERSION 3.15)
project(SWILL_Project VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройки для 32-битной сборки (обязательно для MTA:SA)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(WARNING "Рекомендуется 32-битная сборка для MTA:SA. Используйте -A Win32")
endif()

# Опции сборки
option(BUILD_LOADER "Build SWILL Loader" ON)
option(BUILD_PAYLOAD "Build SWILL Payload (DLL)" ON)
option(GENERATE_PDB "Generate PDB debug files" ON)
option(AUTO_COPY_BINARIES "Auto-copy binaries to bin folder" ON)

# ============================================
# Глобальные настройки компиляции
# ============================================
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)

# Флаги для Release сборки
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /Ot /GL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")

# Отключаем SAFESEH для совместимости
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")

# ============================================
# SWILL Core Library (Static)
# ============================================
add_library(SWILL_Core STATIC
    src/core/HookEngine.cpp
    src/core/PatternScanner.cpp
    src/core/MemoryUtils.cpp
    src/core/AntiTamper.cpp
)

target_include_directories(SWILL_Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SWILL_Core PUBLIC
    psapi
    dbghelp
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
    )

    target_link_libraries(SWILL_Payload PRIVATE
        SWILL_Core
        ws2_32
        wininet
    )

    set_target_properties(SWILL_Payload PROPERTIES
        PREFIX ""
        OUTPUT_NAME "SWILL"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    )
endif()

# ============================================
# SWILL Loader (External Injector)
# ============================================
if(BUILD_LOADER)
    add_executable(SWILL_Loader
        src/loader/main.cpp
        src/loader/Injector.cpp
        src/loader/ProcessUtils.cpp
    )

    target_link_libraries(SWILL_Loader PRIVATE
        SWILL_Core
        psapi
        dbghelp
    )

    set_target_properties(SWILL_Loader PROPERTIES
        OUTPUT_NAME "SWILL_Loader"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    )
endif()

# ============================================
# Автоматизация после сборки
# ============================================
if(AUTO_COPY_BINARIES)
    # Создаем директорию bin
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    
    # Копируем Payload DLL
    if(BUILD_PAYLOAD)
        add_custom_command(TARGET SWILL_Payload POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SWILL_Payload>
                ${CMAKE_BINARY_DIR}/bin/SWILL.dll
            COMMENT "Copying SWILL.dll to bin folder"
        )
        
        if(GENERATE_PDB)
            add_custom_command(TARGET SWILL_Payload POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_PDB_FILE:SWILL_Payload>
                    ${CMAKE_BINARY_DIR}/bin/SWILL.pdb
                COMMENT "Copying SWILL.pdb to bin folder"
            )
        endif()
    endif()
    
    # Копируем Loader EXE
    if(BUILD_LOADER)
        add_custom_command(TARGET SWILL_Loader POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SWILL_Loader>
                ${CMAKE_BINARY_DIR}/bin/SWILL_Loader.exe
            COMMENT "Copying SWILL_Loader.exe to bin folder"
        )
        
        if(GENERATE_PDB)
            add_custom_command(TARGET SWILL_Loader POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_PDB_FILE:SWILL_Loader>
                    ${CMAKE_BINARY_DIR}/bin/SWILL_Loader.pdb
                COMMENT "Copying SWILL_Loader.pdb to bin folder"
            )
        endif()
    endif()
    
    # Создаем README в папке bin
    file(WRITE ${CMAKE_BINARY_DIR}/bin/README.txt
"SWILL Project - Auto-built binaries\n
=====================================\n
Version: ${PROJECT_VERSION}\n
Build Type: $<CONFIG>\n\n
Files:\n
- SWILL.dll         : Main payload DLL for injection\n
- SWILL_Loader.exe  : External injector (optional)\n
- *.pdb             : Debug symbols (if enabled)\n\n
Usage:\n
1. Start GTA San Andreas with MTA client\n
2. Run SWILL_Loader.exe and select the GTA process\n
   OR use your preferred injector to inject SWILL.dll\n\n
Requirements:\n
- MTA:SA 1.6-unstable-0\n
- Windows 7/8/10/11 (x86)\n
- Visual C++ 2015-2022 Redistributable\n\n
Note: This tool is for development and testing purposes only.\n
")
endif()

# ============================================
# Вывод информации о конфигурации
# ============================================
message(STATUS "===========================================")
message(STATUS "SWILL Project Configuration")
message(STATUS "===========================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: $<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x64,x86>")
message(STATUS "Build Loader: ${BUILD_LOADER}")
message(STATUS "Build Payload: ${BUILD_PAYLOAD}")
message(STATUS "Generate PDB: ${GENERATE_PDB}")
message(STATUS "Auto-copy binaries: ${AUTO_COPY_BINARIES}")
message(STATUS "Output directory: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "===========================================")
