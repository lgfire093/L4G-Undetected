cmake_minimum_required(VERSION 3.20)
project(L4G_Driver VERSION 14.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Kernel mode driver settings
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /O2 /MT /D_UNICODE /DUNICODE /DNOMINMAX")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /kernel /driver /integritycheck /guard:cf")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_WIN64 /DAMD64 /DNTDDK /DNTOSKRNL")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:NATIVE /DRIVER:WDM /ENTRY:GsDriverEntry")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INTEGRITYCHECK /GUARD:CF /SAFESEH:NO")
endif()

# Kernel library paths (requires WDK)
set(KERNEL_LIB_PATH "$ENV{WDK_LIB_PATH}")
if(KERNEL_LIB_PATH)
    link_directories(${KERNEL_LIB_PATH})
endif()

# Source files
file(GLOB_RECURSE DRIVER_SOURCES src/*.cpp)

# Driver executable (will be .sys after linking)
add_executable(L4G_Driver ${DRIVER_SOURCES})

# Kernel libraries
target_link_libraries(L4G_Driver PRIVATE
    ntoskrnl
    hal
    wdf
    wdm
)

# Definitions
target_compile_definitions(L4G_Driver PRIVATE
    _KERNEL_MODE
    _NTDDK_
    _NTOSKRNL_
    POOL_TAGGING=1
    DBG=0
)

# Output as .sys
set_target_properties(L4G_Driver PROPERTIES
    SUFFIX ".sys"
    PREFIX ""
)

install(TARGETS L4G_Driver DESTINATION drivers)