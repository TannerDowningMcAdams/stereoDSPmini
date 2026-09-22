# Cortex-M7, double-precision FPU: H7 and tools/bridge.
set(TARGET_FLAGS "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard ")
include("${CMAKE_CURRENT_LIST_DIR}/gcc-arm-none-eabi-common.cmake")
