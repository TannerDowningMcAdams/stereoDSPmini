#include "system.hpp"

void System::init() {

    g0Spi_.init();
    audio_.init();
    analogDryThru_.init();
    analogDryThru_.setVcaValue(0x0000);
    
}

void System::onAudioReady() {



    processor_.processAudio(AudioBuffer audio_.getInputBuffer(), AudioBuffer audio_.getOutputBuffer());

}