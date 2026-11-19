cmake_minimum_required(VERSION 3.20)
project(L4G_Panel VERSION 14.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC specific flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /O2 /MT /D_UNICODE /DUNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /Gy")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LTCG")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Find required libraries
find_package(Threads REQUIRED)

# Source files
file(GLOB_RECURSE PANEL_SOURCES
    src/*.cpp
    src/utils/*.cpp
    src/injection/*.cpp
    src/kernel/*.cpp
    src/evasion/*.cpp
    src/c2/*.cpp
    src/cheat/*.cpp
    src/persistence/*.cpp
)

# Main executable
add_executable(L4G_Panel WIN32 ${PANEL_SOURCES})
target_link_libraries(L4G_Panel PRIVATE Threads::Threads)

# Windows libraries
target_link_libraries(L4G_Panel PRIVATE
    bcrypt
    winhttp
    psapi
    version
    shell32
    ole32
    oleaut32
    uuid
    wbemuuid
    taskschd
    comctl32
)

# Compiler definitions
target_compile_definitions(L4G_Panel PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    L4G_VERSION_MAJOR=14
    L4G_VERSION_MINOR=0
)

# Post-build: copy to output
add_custom_command(TARGET L4G_Panel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:L4G_Panel>
    ${CMAKE_SOURCE_DIR}/../../output/L4G_Panel.exe
    COMMENT "Copying panel to output directory"
)

# Install target
install(TARGETS L4G_Panel DESTINATION bin)