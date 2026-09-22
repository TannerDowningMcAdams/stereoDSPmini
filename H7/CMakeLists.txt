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
set(CMAKE_PROJECT_NAME stereoDSPminiH7_cubeMX)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

set(CMSISCORE ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add CMSIS DSP library sources
add_subdirectory(DSP/CMSIS_DSP)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    
)

# Application sources are listed explicitly: a glob does not re-run when files
# are added, and it silently swept in whatever was left lying in App/Src.
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/analog_dry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/audio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/callbacks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/g0_spi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/relay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/status.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/system.cpp
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    CMSISDSP
    # Add user defined libraries
)
