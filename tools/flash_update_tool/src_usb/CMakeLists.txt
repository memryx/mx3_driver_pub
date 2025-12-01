cmake_minimum_required(VERSION 3.16)
project(memx_usb_update_flash_tool C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(memx_usb_update_flash_tool src/main.c)

# --- Find libusb (Windows) -------------------------------
if(WIN32)
    # Define a variable for the libusb path with a default value
    set(LIBUSB_ROOT_DIR "" CACHE PATH "Path to the libusb-1.0.29 directory")

    # If the user has not provided a path, try a common default.
    # This is optional but can make life easier.
    if(NOT LIBUSB_ROOT_DIR)
		message(FATAL_ERROR "Could not find libusb. Please set LIBUSB_ROOT_DIR to your libusb path.")
    endif()

    # Find the libusb header directory using the variable
    find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h
        PATHS ${LIBUSB_ROOT_DIR}/include)

    # Find the libusb library file using the variable
    find_library(LIBUSB_LIBRARIES NAMES libusb-1.0
        PATHS ${LIBUSB_ROOT_DIR}/VS2022/MS64/dll)

    if(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)
        message(STATUS "Found libusb on Windows")
        target_include_directories(memx_usb_update_flash_tool PRIVATE ${LIBUSB_INCLUDE_DIR})
        target_link_libraries(memx_usb_update_flash_tool PRIVATE ${LIBUSB_LIBRARIES})
    else()
        message(FATAL_ERROR "Could not find libusb. Please set LIBUSB_ROOT_DIR to your libusb path.")
    endif()

# --- Find libusb (Linux) -------------------------------
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBUSB QUIET IMPORTED_TARGET libusb-1.0)
        if(TARGET PkgConfig::LIBUSB)
            target_link_libraries(memx_usb_update_flash_tool PRIVATE PkgConfig::LIBUSB)
            message(STATUS "Found libusb via pkg-config on Linux")
        else()
            message(FATAL_ERROR "Found pkg-config but could not find the libusb-1.0 module.")
        endif()
    else()
        message(FATAL_ERROR "Could not find pkg-config. Please install it or provide libusb paths manually.")
    endif()
endif()

# Warnings
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(memx_usb_update_flash_tool PRIVATE -Wall -Wextra -Wpedantic)
elseif (MSVC)
    target_compile_options(memx_usb_update_flash_tool PRIVATE /W4 /permissive-)
endif()