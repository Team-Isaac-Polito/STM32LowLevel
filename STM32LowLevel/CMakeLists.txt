cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set DEBUG/NDEBUG macros based on build type
# Debug builds: -DDEBUG enables debug code compilation
# Release builds: -DNDEBUG disables debug code (standard assert.h behavior)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
endif()

# Set the project name
set(CMAKE_PROJECT_NAME STM32LowLevel)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Debug library (UART5 LL backend)
add_library(debug_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/Debug/Debug.cpp
)
target_include_directories(debug_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/Debug
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Class/CDC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/App
)
# Inherit STM32G474xx / USE_FULL_LL_DRIVER / USE_HAL_DRIVER from stm32cubemx
target_link_libraries(debug_lib PUBLIC stm32cubemx)

# DynamixelLL library (USART2/3 LL backend, hardware CRC)
add_library(dynamixel_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/DynamixelLL/DynamixelLL.cpp
)
target_include_directories(dynamixel_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/DynamixelLL
)
target_link_libraries(dynamixel_lib PUBLIC debug_lib)

# CAN wrapper library (FDCAN2 register-level runtime TX/RX)
add_library(can_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/CanWrapper/CanWrapper.cpp
)
target_include_directories(can_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/CanWrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_link_libraries(can_lib PUBLIC stm32cubemx)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Shared headers: mod_config.h, definitions.h, communication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    # Debug library public headers (also exposed via debug_lib target)
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/Debug
    # DynamixelLL library public headers
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/DynamixelLL
)

# Forward MODULE_DEFINE to the C/C++ preprocessor
# Usage: cmake --preset Debug -DMODULE_DEFINE=MK2_MOD1
if(NOT DEFINED MODULE_DEFINE)
    message(FATAL_ERROR
        "MODULE_DEFINE is not set. "
        "Pass it on the command line, e.g.:\n"
        "  cmake --preset Debug -DMODULE_DEFINE=MK2_MOD1\n"
        "Valid values: MK2_MOD1 (0x21), MK2_MOD2 (0x22), MK2_MOD3 (0x23)")
endif()
message(STATUS "Building for module: ${MODULE_DEFINE}")

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Module identity — forwarded from -DMODULE_DEFINE=<variant>
    MODULE_DEFINE=${MODULE_DEFINE}
)
# can_lib needs MODULE_DEFINE to resolve CAN_ID in mod_config.h
target_compile_definitions(can_lib PRIVATE MODULE_DEFINE=${MODULE_DEFINE})

# AS5048B AbsoluteEncoder library (I2C1 LL polling)
add_library(encoder_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/AbsoluteEncoder/AbsoluteEncoder.cpp
)
target_include_directories(encoder_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/AbsoluteEncoder
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_link_libraries(encoder_lib PUBLIC stm32cubemx)

# Battery voltage monitor (ADC1 IN1, PA0, LL single-conversion)
add_library(battery_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/Battery/Battery.cpp
)
target_include_directories(battery_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/Battery
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_link_libraries(battery_lib PUBLIC stm32cubemx)

# LSM6DSL IMU library (I2C1 LL polling, complementary filter)
add_library(imu_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/IMU/IMU.cpp
)
target_include_directories(imu_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/IMU
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_link_libraries(imu_lib PUBLIC stm32cubemx)

# USB Device library (CDC ACM VCP)
add_library(usb_device_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Core/Src/usbd_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Core/Src/usbd_ioreq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Core/Src/usbd_ctlreq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Class/CDC/Src/usbd_cdc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/App/usbd_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/App/usbd_desc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/App/usbd_cdc_if.c
)
target_include_directories(usb_device_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/Class/CDC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/USB_Device/App
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_link_libraries(usb_device_lib PUBLIC stm32cubemx)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Enable floating point printf support via newlib
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -u _scanf_float -u _vscanf_float")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -u _scanf_float -u _vscanf_float")

# Enable newlib nano with float printf support
# -u _printf_float forces the linker to include float formatting in vsnprintf/printf
# Without this, %f produces empty output and wastes no extra flash
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,-u,_printf_float
)

# Add flash target (requires dfu-util)
add_custom_target(flash
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMAND dfu-util -d 0483:df11 -a 0 --dfuse-address 0x08000000 -D ${CMAKE_PROJECT_NAME}.bin
    DEPENDS ${CMAKE_PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Flashing ${CMAKE_PROJECT_NAME}.bin to STM32 via DFU"
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    debug_lib
    dynamixel_lib
    can_lib
    encoder_lib
    battery_lib
    imu_lib
    usb_device_lib

    # Add user defined libraries
)
