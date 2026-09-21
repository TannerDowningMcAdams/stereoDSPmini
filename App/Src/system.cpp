#include "system.hpp"
#include "g0_spi.hpp"
#include "processor.hpp"
#include "relay.hpp"
#include <cstring>

void System::init()
{
    g0Spi_.init();
    audio_.init();
    analogDryThru_.init();  
    processor_.init(audio_.kSampleRate);
    processor_.pushControls(translateControls(defaultParams_));
    relay_.rightOn();
    relay_.leftOn();
}

// Signal from Audio that new data is formatted and ready to process
void System::onAudioReady()
{
    // Pass audio I/O buffers to Processor; audio_.outputBuffer_ is written in place
    processor_.processAudioBlock(audio_.getInputBuffer(), audio_.getOutputBuffer());
}

void System::poll()
{
    audio_.serviceErrors();
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
    // Update pending parameters for audio processing
    processor_.pushControls(newControls);
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

