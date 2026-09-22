#pragma once

#include <stdint.h>

// Perceptual brightness correction for 8-bit PWM (ARR = 255)
uint8_t ledGamma(uint8_t level);
