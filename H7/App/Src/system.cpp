#include "system.hpp"
#include "protocol_version.h"
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
    const G0Spi::Config         spiConfig   { &hspi2, G0_EXTI_Pin };
    const Audio::Config         audioConfig { &hsai_BlockA1, &hsai_BlockB1,
                                              { CODEC_NRST_GPIO_Port, CODEC_NRST_Pin } };

    // Defaults are pushed before SPI starts: pushControls() assumes one producer,
    // and after this the SPI ISR is the only one.
    processor_.init(audio_.kSampleRate);
    processor_.pushControls(toProcessorControls(G0Spi::defaultControls()));
    relay_.init(relayConfig);
    analogDryThru_.init(dryConfig);
    g0Spi_.init(spiConfig);
#if STEREODSPMINI_G0_IMAGE
    updateG0();
#endif
    // Processor is ready before the first block can be published.
    audio_.setProcessor(&processor_);
    audio_.init(audioConfig);
    // Relays on and VCA at its init value until the bypass controller (M2) owns them.
    relay_.rightOn();
    relay_.leftOn();
}

void System::poll()
{
    audio_.serviceErrors();
    audio_.serviceBlock();
}

#if STEREODSPMINI_G0_IMAGE
// Programs the G0 with the embedded image when its app reports another version, or when
// it is already in its bootloader (AUX held at power-up, or blank flash). Runs before
// audio starts, since programming blocks for seconds.
void System::updateG0()
{
    if (g0Bootloader_.init({ &huart3 }) != Status::OK) { return; }

    const uint32_t start = HAL_GetTick();
    while (!g0Spi_.g0Seen() && (HAL_GetTick() - start) < kG0LinkWindowMs) {}

    if (g0Spi_.g0Seen())
    {
        if (g0Spi_.g0ProtocolVersion() == PROTOCOL_VERSION && g0Spi_.g0FwVersion() == G0_FW_VERSION)
        {
            return;
        }
        g0Spi_.setBootloaderRequest(true);
        HAL_Delay(kG0RequestMs);
        g0Spi_.setBootloaderRequest(false);
        // The transfer armed at the G0's last frame still holds the magic. Left in
        // place, the new image would receive it first and return to the bootloader.
        g0Spi_.restartFrame();
    }

    if (g0Bootloader_.probe(kG0ProbeWindowMs))
    {
        (void) g0Bootloader_.program(g0_image_start, g0ImageSize());
    }
}
#endif

// SPI DMA callback, once per G0 frame (1 ms); see callbacks.cpp for origin
void System::spiTxRxComplete()
{
    // Only a valid frame drives anything. Until the G0's first one, the defaults
    // set in init() stand.
    if (!g0Spi_.txRxComplete()) { return; }

    // A refused set is dropped, not retried: the next frame 1 ms later is fresher.
    (void) processor_.pushControls(toProcessorControls(g0Spi_.controls()));
}

ProcessorControls System::toProcessorControls(const G0Spi::Controls& controls)
{
    ProcessorControls out;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++)
    {
        out.params[i] = controls.param[i] * (1.0f / 65535.0f);
    }
    out.discrete   = controls.discrete;
    out.tempoHz    = controls.tempoHz;
    out.tempoPhase = controls.tempoPhase;
    return out;
}
