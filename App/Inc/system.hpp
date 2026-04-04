#pragma once
#include "audio.hpp"
#include "frame.hpp"
#include "ui_params.hpp"
#include "relay.hpp"
#include "g0_spi.hpp"
#include "analog_dry.hpp"
#include "processor.hpp"
#include <cstdint>
#ifdef __cplusplus
extern "C" {
#endif
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

class System {
public:

    System() = default;
    ~System() = default;

    void init();
    void onAudioReady();
    void onParamsReady();

    inline void audioRxHalfComplete()   { audio_.rxHalfComplete(); }
    inline void audioRxComplete()       { audio_.rxComplete(); }
    inline void audioTxHalfComplete()   { audio_.txHalfComplete(); }
    inline void audioTxComplete()       { audio_.txComplete(); }
    void audioErrorCallback();
    void spiTxRxComplete();
    void spiErrorCallback();

    
private:

    Audio audio_;
    G0Spi g0Spi_;
    Relay relay_;
    AnalogDryThru analogDryThru_;
    Processor processor_;
    uiParams uiParams_;
    bool uiParamsLocked_;

    processorControls translateControls(const uiParams &params);

} ;