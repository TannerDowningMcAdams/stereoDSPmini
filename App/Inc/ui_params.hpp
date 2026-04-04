#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

struct uiParams{

    float potentiometers[5];
    bool relay_L;
    bool relay_R;
    uint8_t mode_switch;
    uint16_t vca_value; // Only 12 bits used
    float beats_per_second;
    uint16_t clock_phase;

};