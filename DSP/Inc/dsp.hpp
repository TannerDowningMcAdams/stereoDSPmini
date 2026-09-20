#pragma once

// Selects the math backend the DSP layer builds against. The CMSIS path is the
// only one implemented; the alternative exists so the same DSP sources can be
// compiled off-target (JUCE, tests) later.
#define DSP_USE_CMSIS

#ifdef DSP_USE_CMSIS
#include "arm_math.h"
#else
// TODO: #include std lib, eigen, etc. for JUCE compilation
#endif
