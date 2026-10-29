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

# Set the project name
set(CMAKE_PROJECT_NAME STM32_PROJECT)

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
    Core/Src/valve_control.c
    Core/Src/valve_port.c
    Core/Src/rs485_rx.c
    Core/Src/battery_monitor.c
    Core/Src/battery_adc_port.c
    Core/Src/app_status.c
    Core/Src/power_manager.c
    Core/Src/power_port.c
    Core/Src/power_rtc.c
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Stage3A profile is independent of optimization/DEBUG symbols.
option(VALVE_LOW_POWER "Enable ordinary Sleep with TIM2 timebase" OFF)
option(VALVE_STOP1 "Enable GPIO-only Stop1; overrides VALVE_LOW_POWER" OFF)
if(VALVE_STOP1)
    set(VALVE_POWER_PROFILE 2)
elseif(VALVE_LOW_POWER)
    set(VALVE_POWER_PROFILE 1)
else()
    set(VALVE_POWER_PROFILE 0)
endif()
# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    POWER_LOW_POWER_MODE=${VALVE_POWER_PROFILE}
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
