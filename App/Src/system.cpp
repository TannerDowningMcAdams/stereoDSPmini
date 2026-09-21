#include "system.hpp"
#include "g0_spi.hpp"
#include "processor.hpp"
#include "relay.hpp"
#include <cstring>

void System::init()
{
    // Defaults are pushed before SPI starts: pushControls() assumes one producer,
    // and after this the SPI ISR is the only one.
    processor_.init(audio_.kSampleRate);
    processor_.pushControls(translateControls(defaultParams_));
    g0Spi_.init();
    analogDryThru_.init();
    // Processor is ready before the first block can be published.
    audio_.setProcessor(&processor_);
    audio_.init();
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
    // G0 SPI callback unpacks data and updates internal g0Spi_.params_
    g0Spi_.txRxComplete();
    // Update VCA and Relays
    analogDryThru_.setVcaValue(g0Spi_.params_.vcaValue);
    g0Spi_.params_.relayL ? relay_.leftOn() : relay_.leftOff();
    g0Spi_.params_.relayR ? relay_.rightOn() : relay_.rightOff();
    // Extract control data relevant to Processor from g0Spi_.params_
    ProcessorControls newControls = translateControls(g0Spi_.params_);
    // A refused set is dropped, not retried: the next tick 10 ms later is fresher.
    (void) processor_.pushControls(newControls);
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

