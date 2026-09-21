#include "analog_dry.hpp"

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
