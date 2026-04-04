#include "system.hpp"
#include "g0_spi.hpp"
#include "processor.hpp"
#include <cstring>

void System::init() {

    g0Spi_.init();
    audio_.init();
    analogDryThru_.init();
    
}

void System::onAudioReady() {

    processor_.processAudioBlock(audio_.getInputBuffer(), audio_.getOutputBuffer());

}

void System::spiTxRxComplete() { 

    // G0 SPI callback updates internal uiParams
    g0Spi_.txRxComplete(); 
    analogDryThru_.setVcaValue(g0Spi_.params_.vcaValue);
    g0Spi_.params_.relayL ? relay_.leftOn() : relay_.leftOff();
    g0Spi_.params_.relayR ? relay_.rightOn() : relay_.rightOff();
    ProcessorControls newControls = translateControls(g0Spi_.params_);
    // Update pending parameters for audio processing
    processor_.pushControls(newControls);

}

ProcessorControls translateControls(const uiParams &params) {

    ProcessorControls controls;
    std::memcpy(controls.potentiometers, params.potentiometers, sizeof(controls.potentiometers));
    controls.effectMode = params.modeSwitch;
    controls.beatsPerSecond = params.beatsPerSecond;
    controls.clockPhase = params.clockPhase;

}
 