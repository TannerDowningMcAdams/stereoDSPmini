#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "audio.hpp"
#include "frame.hpp"
#include "relay.hpp"
#include "g0_spi.hpp"
#include "analog_dry.hpp"
#include "stm32h7xx_hal.h"
#include "processor.hpp"
#ifdef __cplusplus
}
#endif

#include <cstdint>

class System {
public:

    System() = default;
    ~System() = default;

    void init();
    void onAudioReady();

    inline void audioRxHalfComplete()   { audio_.rxHalfComplete(); }
    inline void audioRxComplete()       { audio_.rxComplete(); }
    inline void audioTxHalfComplete()   { audio_.txHalfComplete(); }
    inline void audioTxComplete()       { audio_.txComplete(); }
    inline void audioErrorCallback();
    inline void spiTxRxComplete()       { g0Spi_.txRxComplete(); }
    inline void spiErrorCallback();

private:

    Audio audio_;
    G0Spi g0Spi_;
    Relay relay_;
    AnalogDryThru analogDryThru_;
    Processor processor_;

} ;