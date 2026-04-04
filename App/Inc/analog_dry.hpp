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

    void setVcaValue(uint16_t dryStrength);
    uint16_t getCurrentValue() { return currentValue_; }

private:

    OPAMP_HandleTypeDef *opampHandle_ = &hopamp1;
    DAC_HandleTypeDef *dacHandle_ = &hdac1;
    uint32_t dacChannel_ = DAC_CHANNEL_1;
    uint16_t currentValue_;

} ;