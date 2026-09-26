#pragma once

#include <cstddef>
#include <cstdint>

// G0 application image, linked in by the superbuild (STEREODSPMINI_G0_IMAGE is set when present).
extern "C" const uint8_t g0_image_start[];
extern "C" const uint8_t g0_image_end[];

inline std::size_t g0ImageSize() { return static_cast<std::size_t>(g0_image_end - g0_image_start); }
