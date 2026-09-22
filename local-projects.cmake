# One ExternalProject per firmware image. Binary dirs match each project's standalone preset layout.
set(STEREODSPMINI_SUBBUILD_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
    -DSTM32CLT_PATH=${STM32CLT_PATH}
)

# stereodspmini_add_image(<name> <dir> <toolchain> [EXCLUDE_FROM_ALL] [DEPENDS ...] [CMAKE_ARGS ...])
function(stereodspmini_add_image name source_dir toolchain)
    cmake_parse_arguments(IMG "EXCLUDE_FROM_ALL" "" "DEPENDS;CMAKE_ARGS" ${ARGN})
    set(_binary_dir ${PROJECT_SOURCE_DIR}/${source_dir}/build/${CMAKE_BUILD_TYPE})
    set(_extra)
    if(IMG_DEPENDS)
        list(APPEND _extra DEPENDS ${IMG_DEPENDS})
    endif()
    if(IMG_EXCLUDE_FROM_ALL)
        list(APPEND _extra EXCLUDE_FROM_ALL true)
    endif()

    ExternalProject_Add(${name}
        SOURCE_DIR                 ${PROJECT_SOURCE_DIR}/${source_dir}
        BINARY_DIR                 ${_binary_dir}
        PREFIX                     ${CMAKE_BINARY_DIR}/${name}
        CMAKE_ARGS                 -DCMAKE_TOOLCHAIN_FILE=${PROJECT_SOURCE_DIR}/cmake/${toolchain}
                                   ${STEREODSPMINI_SUBBUILD_ARGS}
                                   ${IMG_CMAKE_ARGS}
        CONFIGURE_HANDLED_BY_BUILD true
        BUILD_ALWAYS               true
        INSTALL_COMMAND            ""
        USES_TERMINAL_CONFIGURE    true
        USES_TERMINAL_BUILD        true
        ${_extra}
    )
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${_binary_dir})
endfunction()

set(G0_IMAGE ${PROJECT_SOURCE_DIR}/G0/build/${CMAKE_BUILD_TYPE}/stereoDSPminiG0_cubeMX.bin)

stereodspmini_add_image(stereoDSPmini_G0 G0 gcc-arm-none-eabi-m0plus.cmake)
stereodspmini_add_image(stereoDSPmini_H7 H7 gcc-arm-none-eabi-m7.cmake
    DEPENDS    stereoDSPmini_G0
    CMAKE_ARGS -DG0_IMAGE=${G0_IMAGE}
)

# Bench and recovery tool; build with --target stereoDSPmini_bridge.
stereodspmini_add_image(stereoDSPmini_bridge tools/bridge gcc-arm-none-eabi-m7.cmake EXCLUDE_FROM_ALL)
