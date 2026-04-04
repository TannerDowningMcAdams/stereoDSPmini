#include "processor.hpp"
#include <cstdint>
#include <cstring>

void Processor::init() {

// Initialize DSP, buffers, etc

}

void Processor::processAudioBlock(AudioBuffer input, AudioBuffer output) {

    if(controlsReady_) {

        activeControls_ = pendingControls_;
        updateAlgorithmParams();
        controlsReady_ = false;

    }

    for (uint16_t i = 0; i < input.size; i++) {

        output[i] = processLeftRight(input[i]);

    }

}

FloatFrame Processor::processLeftRight(FloatFrame frame) {

    // Per-sample processing applied to frame.left and frame.right

    return frame;

}

void Processor::updateAlgorithmParams() {

    // assign algorithm parameters
    // e.g. filter_cutoff = 20000.0f * activeControls_.potentiometers[0];

}

void Processor::pushControls(const processorControls &controls) {

    pendingControls_ = controls;
    controlsReady_ = true;

}