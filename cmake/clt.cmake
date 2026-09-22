# STM32CubeCLT is the only source of build tools. Included by the superbuild and every toolchain file.
# Override the install with -DSTM32CLT_PATH=<dir> where the environment variable is not set.

if(NOT STM32CLT_PATH)
    set(STM32CLT_PATH "$ENV{STM32CLT_PATH}")
endif()
if(NOT STM32CLT_PATH)
    message(FATAL_ERROR "STM32CubeCLT not found: set the STM32CLT_PATH environment variable or pass -DSTM32CLT_PATH.")
endif()
file(TO_CMAKE_PATH "${STM32CLT_PATH}" CLT_ROOT)
set(STM32CLT_PATH "${CLT_ROOT}" CACHE PATH "STM32CubeCLT install root" FORCE)

# try_compile() re-reads the toolchain file in a fresh context; carry the -D override into it.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES STM32CLT_PATH)

if(CMAKE_HOST_WIN32)
    set(CLT_EXE ".exe")
else()
    set(CLT_EXE "")
endif()

set(CLT_GCC_PREFIX "${CLT_ROOT}/GNU-tools-for-STM32/bin/arm-none-eabi-")
set(CLT_CMAKE      "${CLT_ROOT}/CMake/bin/cmake${CLT_EXE}")
set(CLT_NINJA      "${CLT_ROOT}/Ninja/bin/ninja${CLT_EXE}")

foreach(_tool "${CLT_GCC_PREFIX}gcc${CLT_EXE}" "${CLT_CMAKE}" "${CLT_NINJA}")
    if(NOT EXISTS "${_tool}")
        message(FATAL_ERROR "STM32CubeCLT at ${CLT_ROOT} is missing ${_tool}. No fallback to PATH or the VS Code bundles.")
    endif()
endforeach()

# FORCE displaces the -DCMAKE_MAKE_PROGRAM=<bundled ninja> that the VS Code extension passes.
set(CMAKE_MAKE_PROGRAM "${CLT_NINJA}" CACHE FILEPATH "Ninja from STM32CubeCLT" FORCE)

# Sub-builds must run under CLT's cmake; the superbuild pins it for them.
function(clt_require_clt_cmake)
    get_property(_in_try_compile GLOBAL PROPERTY IN_TRY_COMPILE)
    if(_in_try_compile)
        return()
    endif()
    file(TO_CMAKE_PATH "${CMAKE_COMMAND}" _running)
    set(_expected "${CLT_CMAKE}")
    if(CMAKE_HOST_WIN32)
        string(TOLOWER "${_running}" _running)
        string(TOLOWER "${_expected}" _expected)
    endif()
    if(NOT _running STREQUAL _expected)
        message(FATAL_ERROR
            "This build is running under ${CMAKE_COMMAND}, not STM32CubeCLT's ${CLT_CMAKE}.\n"
            "Configure from the repository root, or delete this build directory if it was made by another cmake.")
    endif()
endfunction()
