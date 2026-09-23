# Cortex-M0+, no FPU: G0.
set(TARGET_FLAGS "-mcpu=cortex-m0plus ")
include("${CMAKE_CURRENT_LIST_DIR}/gcc-arm-none-eabi-common.cmake")

# The G0 has 56 KB for the app; -Og keeps Debug near Release size and still debuggable.
set(CMAKE_C_FLAGS_DEBUG "-Og -g3")
