#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

struct uiParams {

    float potentiometers[5];
    bool relayL;
    bool relayR;
    uint8_t modeSwitch;
    uint16_t vcaValue; // Only 12 bits used
    float beatsPerSecond;
    uint16_t clockPhase;

};