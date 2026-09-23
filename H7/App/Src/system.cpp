#include "system.hpp"
#include <cstring>
#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
#include "sai.h"
#include "spi.h"
#include "dac.h"
#include "opamp.h"
#include "usart.h"
#ifdef __cplusplus
}
#endif
#if STEREODSPMINI_G0_IMAGE
#include "g0_image.hpp"
#endif

void System::init()
{
    // Board wiring: every peripheral handle and pin the drivers use, in one place.
    // Locals, since GPIOx is a cast from an integer and cannot be constexpr.
    const Relay::Config relayConfig {
        { RELAY_L_GPIO_Port, RELAY_L_Pin },
        { RELAY_R_GPIO_Port, RELAY_R_Pin },
    };
    const AnalogDryThru::Config dryConfig   { &hdac1, DAC_CHANNEL_1, &hopamp1 };
    const G0Spi::Config         spiConfig   { &hspi2 };
    const Audio::Config         audioConfig { &hsai_BlockA1, &hsai_BlockB1,
                                              { CODEC_NRST_GPIO_Port, CODEC_NRST_Pin } };

#if STEREODSPMINI_G0_IMAGE
    // Until protocol v2 can command it: a G0 already in its bootloader (AUX, FTSW_L, held
    // at power-up, or blank flash) is programmed with the embedded image before audio starts.
    if (g0Bootloader_.init({ &huart3 }) == Status::OK && g0Bootloader_.probe(kG0ProbeWindowMs))
    {
        (void) g0Bootloader_.program(g0_image_start, g0ImageSize());
    }
#endif

    // Defaults are pushed before SPI starts: pushControls() assumes one producer,
    // and after this the SPI ISR is the only one.
    processor_.init(audio_.kSampleRate);
    processor_.pushControls(translateControls(defaultParams_));
    // The SPI ISR drives both of these, so they are configured before it starts.
    relay_.init(relayConfig);
    analogDryThru_.init(dryConfig);
    g0Spi_.init(spiConfig);
    // Processor is ready before the first block can be published.
    audio_.setProcessor(&processor_);
    audio_.init(audioConfig);
    relay_.rightOn();
    relay_.leftOn();
}

void System::poll()
{
    audio_.serviceErrors();
    audio_.serviceBlock();
}

// SPI DMA callback occurs every 10ms; see callbacks.hpp for origin
void System::spiTxRxComplete()
{ 
    // Only a valid control packet drives anything. Until the G0's first one, the
    // defaults set in init() stand.
    if (!g0Spi_.txRxComplete()) { return; }

    const uiParams& params = g0Spi_.params();
    analogDryThru_.setVcaValue(params.vcaValue);
    params.relayL ? relay_.leftOn() : relay_.leftOff();
    params.relayR ? relay_.rightOn() : relay_.rightOff();
    // A refused set is dropped, not retried: the next tick 10 ms later is fresher.
    (void) processor_.pushControls(translateControls(params));
}

ProcessorControls System::translateControls(const uiParams &params)
{
    ProcessorControls controls;
    std::memcpy(controls.potentiometers, params.potentiometers, sizeof(controls.potentiometers));
    controls.effectMode = params.modeSwitch;
    controls.beatsPerSecond = params.beatsPerSecond;
    controls.clockPhase = params.clockPhase;
    return controls;
}

