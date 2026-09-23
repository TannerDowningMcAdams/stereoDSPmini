#pragma once

#include <stdbool.h>
#include <stdint.h>

// Potentiometer conditioning: hysteresis, normalisation, and takeover. A pot drives
// its param only after it moves past the takeover threshold from its baseline, so a
// recalled or restored value holds until the player touches that pot. No HAL.

#define POT_MAX 8u

void potInit(uint8_t count);

// 12-bit reading from one completed ADC scan. The first scan sets the baseline.
void potUpdate(uint8_t index, uint16_t raw);

// Re-takes every baseline: on preset apply, state restore and entering settings.
void potRebaseline(void);

bool     potMoved(uint8_t index);
// Normalized 0..65535, with a small dead zone at each end so both extremes are reachable.
uint16_t potValue(uint8_t index);
