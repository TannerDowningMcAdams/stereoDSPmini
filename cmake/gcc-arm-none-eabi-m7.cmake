# Cortex-M7, double-precision FPU: H7 and tools/bridge.
set(TARGET_FLAGS "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard ")
include("${CMAKE_CURRENT_LIST_DIR}/gcc-arm-none-eabi-common.cmake")

# DSP must keep up with the audio block in Debug too, so an engine that fits the budget
# in Release also fits it here; -Og stays debuggable.
set(CMAKE_C_FLAGS_DEBUG "-Og -g3")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g3")
