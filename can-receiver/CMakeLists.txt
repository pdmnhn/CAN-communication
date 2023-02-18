# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/pdmnhn/Documents/pico-project/pico-sdk")

set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(can-receiver C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_subdirectory(pico-ssd1306)
add_executable(can-receiver mcp2515.cpp can-receiver.cpp )

pico_set_program_name(can-receiver "can-receiver")
pico_set_program_version(can-receiver "0.1")

pico_enable_stdio_uart(can-receiver 0)
pico_enable_stdio_usb(can-receiver 1)

# Add the standard library to the build
target_link_libraries(can-receiver
        pico_stdlib
        pico_ssd1306
        hardware_i2c
        hardware_spi)

# Add the standard include files to the build
target_include_directories(can-receiver PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(can-receiver
        )

pico_add_extra_outputs(can-receiver)

