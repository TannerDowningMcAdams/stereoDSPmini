#pragma once
#include <cstdint>

struct uiParams {

    float potentiometers[5];
    bool relayL;
    bool relayR;
    bool killWet;
    uint8_t modeSwitch;
    uint16_t vcaValue; // Only 12 bits used
    float beatsPerSecond;
    uint16_t clockPhase;

};