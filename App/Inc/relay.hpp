#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "gpio.h"
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

#include <cstdint>

class Relay {
public:

    Relay() = default;
    ~Relay() = default;

    enum class RelayStatus { OFF, ON };

    void leftOff();
    void leftOn();
    void rightOff();
    void rightOn();
    
    RelayStatus getLeftStatus(){ return leftStatus; }
    RelayStatus getRightStatus(){ return rightStatus; }
    
private:

    RelayStatus leftStatus = RelayStatus::OFF;
    RelayStatus rightStatus = RelayStatus::OFF;

} ;