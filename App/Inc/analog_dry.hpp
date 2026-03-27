#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "gpio.h"
#include "dac.h"
#include "opamp.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_dac.h"
#include "stm32h7xx_hal_opamp.h"
#ifdef __cplusplus
}
#endif

#include <cstdint>

class AnalogDryThru {
public:

    AnalogDryThru() = default;
    ~AnalogDryThru() = default;

    enum class AnalogDryStatus { OFF, ON };

    void init();

    void setVcaValue(uint16_t dry_strength);
    uint16_t getCurrentValue() { return current_value_; }

private:

    OPAMP_HandleTypeDef *opamp_handle_ = &hopamp1;
    DAC_HandleTypeDef *dac_handle_ = &hdac1;
    uint32_t dac_channel_ = DAC_CHANNEL_1;
    uint16_t current_value_;

} ;