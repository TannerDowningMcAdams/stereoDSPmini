#include "analog_dry.hpp"
#include "main.h"
#include "stm32h7xx_hal_def.h"
#include <cstdint>
#include <cstring>

void AnalogDryThru::init() {

    // DAC is internally routed to OPAMP peripheral
    HAL_DAC_Start(dac_handle_, dac_channel_);
    HAL_Delay(20);
    HAL_DAC_SetValue(dac_handle_, dac_channel_, DAC_ALIGN_12B_R, 0);
    HAL_Delay(20);
    HAL_OPAMP_Start(opamp_handle_);
    current_value_ = 0;

}

void AnalogDryThru::setVcaValue(uint16_t dry_strength) {

    // 12 bit mask
    uint16_t dry_strength_mask = dry_strength & (0x0FFF);
    HAL_DAC_SetValue(dac_handle_, dac_channel_, DAC_ALIGN_12B_R, dry_strength_mask);
    current_value_ = dry_strength_mask;

}
