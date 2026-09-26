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
    // Linear amplitude 0..1 through the SSI2162 law.
    void setGain(float gain);
    uint16_t getCurrentValue() { return currentValue_; }

private:

    Config config_ {};
    uint16_t currentValue_ = 0;

    // SSI2162 control is -33 mV/dB and the buffered DAC spans 0..3.3 V, so full
    // scale is 0 dB and 0 V is -100 dB, linear in dB between.
    static constexpr uint16_t kFullScale = 4095u;
    static constexpr float    kRangeDb   = 100.0f;
    static constexpr float    kMinGain   = 1.0e-5f;   // -100 dB

} ;
