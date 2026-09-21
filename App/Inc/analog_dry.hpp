#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

#include <cstdint>

class AnalogDryThru {
public:

    struct Config
    {
        DAC_HandleTypeDef*   dac;
        uint32_t             dacChannel;    // DAC_CHANNEL_x
        OPAMP_HandleTypeDef* opamp;         // DAC is routed to it internally
    };

    AnalogDryThru() = default;
    ~AnalogDryThru() = default;

    enum class AnalogDryStatus { OFF, ON };

    void init(const Config& config);

    void setVcaValue(uint16_t dryStrength);
    uint16_t getCurrentValue() { return currentValue_; }

private:

    Config config_ {};
    uint16_t currentValue_ = 0;

} ;
