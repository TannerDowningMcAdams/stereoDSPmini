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

# Linker script: the shared toolchain files carry no -T.
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32H743xx_FLASH.ld)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -T${LINKER_SCRIPT})
set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS ${LINKER_SCRIPT})

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
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/bypass_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/callbacks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/g0_bootloader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/g0_spi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/relay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/status.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/tempo_follower.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/test_delay.cpp
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

# Embedded G0 image, passed in by the superbuild. Converted at build time, so a clean build waits for the G0.
if(G0_IMAGE)
    set(G0_IMAGE_COPY ${CMAKE_CURRENT_BINARY_DIR}/g0_image.bin)
    set(G0_IMAGE_OBJ  ${CMAKE_CURRENT_BINARY_DIR}/g0_image.o)
    # objcopy names the symbols after the input path, hence the local copy and relative name.
    add_custom_command(
        OUTPUT  ${G0_IMAGE_OBJ}
        COMMAND ${CMAKE_COMMAND} -E copy ${G0_IMAGE} ${G0_IMAGE_COPY}
        COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-littlearm -B arm
                --rename-section .data=.rodata.g0_image,alloc,load,readonly,data,contents
                --set-section-alignment .rodata.g0_image=4
                --redefine-sym _binary_g0_image_bin_start=g0_image_start
                --redefine-sym _binary_g0_image_bin_end=g0_image_end
                --strip-symbol _binary_g0_image_bin_size
                g0_image.bin g0_image.o
        DEPENDS ${G0_IMAGE}
        BYPRODUCTS ${G0_IMAGE_COPY}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "H7: embedding G0 image ${G0_IMAGE}"
    )
    set_source_files_properties(${G0_IMAGE_OBJ} PROPERTIES EXTERNAL_OBJECT true GENERATED true)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${G0_IMAGE_OBJ})
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE STEREODSPMINI_G0_IMAGE=1)
else()
    message(STATUS "G0 image not embedded: configure from the repository root to include it")
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    CMSISDSP
    # Add user defined libraries
)
