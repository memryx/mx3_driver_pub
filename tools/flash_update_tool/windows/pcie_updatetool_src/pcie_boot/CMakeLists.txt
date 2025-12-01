
cmake_minimum_required(VERSION 3.15)
project(pcieupdateflash_win)

set(CMAKE_C_STANDARD 11)

add_definitions(-DUNICODE -D_UNICODE)

# Set Windows SDK path and target platform
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_GENERATOR_PLATFORM x64)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(pcieupdateflash_win pcieupdateflash_win.cpp)

find_library(UDRIVER_LIB
    NAMES udriver
    PATHS
        "C:/Program Files/MemryX/lib"

    NO_DEFAULT_PATH
)
if (NOT UDRIVER_LIB)
    message(FATAL_ERROR "Cannot find udriver.lib")
endif()

target_link_libraries(pcieupdateflash_win PRIVATE
    ${UDRIVER_LIB}
)

target_include_directories(pcieupdateflash_win PRIVATE
    "C:/Program Files/MemryX/include"
)