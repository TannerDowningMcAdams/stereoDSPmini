#include "analog_dry.hpp"
#include <cmath>

void AnalogDryThru::init(const Config& config)
{
    config_ = config;

    // DAC is internally routed to OPAMP peripheral
    HAL_DAC_Start(config_.dac, config_.dacChannel);
    HAL_Delay(20);
    HAL_DAC_SetValue(config_.dac, config_.dacChannel, DAC_ALIGN_12B_R, 0);
    HAL_Delay(20);
    HAL_OPAMP_Start(config_.opamp);
    currentValue_ = 0;
}

// Set SSI2162 (dry signal) to specified value
void AnalogDryThru::setVcaValue(uint16_t dryStrength)
{
    // 12 bit mask
    uint16_t dryStrengthMask = dryStrength & (0x0FFF);
    HAL_DAC_SetValue(config_.dac, config_.dacChannel, DAC_ALIGN_12B_R, dryStrengthMask);
    currentValue_ = dryStrengthMask;
}

void AnalogDryThru::setGain(float gain)
{
    if (gain <= kMinGain) { setVcaValue(0u); return; }
    if (gain >= 1.0f)     { setVcaValue(kFullScale); return; }
    const float db = 20.0f * log10f(gain);
    setVcaValue(static_cast<uint16_t>(kFullScale * (1.0f + db / kRangeDb) + 0.5f));
}
