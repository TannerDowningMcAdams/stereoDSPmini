# One ExternalProject per firmware image. Binary dirs match each project's standalone preset layout.
set(STEREODSPMINI_SUBBUILD_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
    -DSTM32CLT_PATH=${STM32CLT_PATH}
)

function(stereodspmini_add_image name source_dir toolchain)
    ExternalProject_Add(${name}
        SOURCE_DIR                 ${PROJECT_SOURCE_DIR}/${source_dir}
        BINARY_DIR                 ${PROJECT_SOURCE_DIR}/${source_dir}/build/${CMAKE_BUILD_TYPE}
        PREFIX                     ${CMAKE_BINARY_DIR}/${name}
        CMAKE_ARGS                 -DCMAKE_TOOLCHAIN_FILE=${PROJECT_SOURCE_DIR}/cmake/${toolchain}
                                   ${STEREODSPMINI_SUBBUILD_ARGS}
        CONFIGURE_HANDLED_BY_BUILD true
        BUILD_ALWAYS               true
        INSTALL_COMMAND            ""
        USES_TERMINAL_CONFIGURE    true
        USES_TERMINAL_BUILD        true
        ${ARGN}
    )
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
        "${PROJECT_SOURCE_DIR}/${source_dir}/build/${CMAKE_BUILD_TYPE}")
endfunction()

stereodspmini_add_image(stereoDSPmini_G0 G0 gcc-arm-none-eabi-m0plus.cmake)
stereodspmini_add_image(stereoDSPmini_H7 H7 gcc-arm-none-eabi-m7.cmake)

# Bench and recovery tool; build with --target stereoDSPmini_bridge.
stereodspmini_add_image(stereoDSPmini_bridge tools/bridge gcc-arm-none-eabi-m7.cmake EXCLUDE_FROM_ALL true)
