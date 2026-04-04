#include "analog_dry.hpp"
#include "main.h"
#include "stm32h7xx_hal_def.h"
#include <cstdint>
#include <cstring>

void AnalogDryThru::init() {

    // DAC is internally routed to OPAMP peripheral
    HAL_DAC_Start(dacHandle_, dacChannel_);
    HAL_Delay(20);
    HAL_DAC_SetValue(dacHandle_, dacChannel_, DAC_ALIGN_12B_R, 0);
    HAL_Delay(20);
    HAL_OPAMP_Start(opampHandle_);
    currentValue_ = 0;

}

void AnalogDryThru::setVcaValue(uint16_t dryStrength) {

    // 12 bit mask
    uint16_t dryStrengthMask = dryStrength & (0x0FFF);
    HAL_DAC_SetValue(dacHandle_, dacChannel_, DAC_ALIGN_12B_R, dryStrengthMask);
    currentValue_ = dryStrengthMask;

}
