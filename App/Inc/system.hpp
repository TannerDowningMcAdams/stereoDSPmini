#pragma once
#include "audio.hpp"
#include "audio_buffer.hpp"
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
    // Thread-mode work, called from the App_Run() loop.
    void poll();

    inline void audioRxHalfComplete()   { audio_.rxHalfComplete(); }
    inline void audioRxComplete()       { audio_.rxComplete(); }
    inline void audioTxHalfComplete()   { audio_.txHalfComplete(); }
    inline void audioTxComplete()       { audio_.txComplete(); }
    inline void audioErrorCallback(SAI_HandleTypeDef* hsai) { audio_.audioErrorHandler(hsai); }
    void spiTxRxComplete();
    void spiErrorCallback()             { g0Spi_.spiErrorHandler(); }

private:

    Audio audio_;
    G0Spi g0Spi_;
    Relay relay_;
    AnalogDryThru analogDryThru_;
    Processor processor_;
    static constexpr uiParams defaultParams_ = {
        {0.5, 0.5, 0.5, 0.5, 0.5},
        true,
        true,
        true,
        0,
        0,
        2.0f,
        0
    };

    ProcessorControls translateControls(const uiParams &params);
    
} ;