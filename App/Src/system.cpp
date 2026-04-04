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
    analogDryThru_.setVcaValue(g0Spi_.params_.vca_value);
    g0Spi_.params_.relay_L ? relay_.leftOn() : relay_.leftOff();
    g0Spi_.params_.relay_R ? relay_.rightOn() : relay_.rightOff();
    processorControls newControls = translateControls(g0Spi_.params_);
    // Update pending parameters for audio processing
    processor_.pushControls(newControls);

}

processorControls translateControls(const uiParams &params) {

    processorControls controls;
    std::memcpy(controls.potentiometers, params.potentiometers, sizeof(controls.potentiometers));
    controls.effect_mode = params.mode_switch;
    controls.beats_per_second = params.beats_per_second;
    controls.clock_phase = params.clock_phase;

}
 