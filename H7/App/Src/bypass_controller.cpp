#include "bypass_controller.hpp"
#include "spi_protocol.h"

uint16_t BypassController::blocksFor(float ms, uint32_t sampleRate) const
{
    const float blocks = ms * 0.001f * sampleRate / blockSize_;
    return static_cast<uint16_t>(blocks + 0.999f);
}

void BypassController::init(const Config& config)
{
    blockSize_ = (config.blockSize > kMaxBlockSize) ? kMaxBlockSize : config.blockSize;
    relay_.init(config.relay);
    relay_.leftOff();
    relay_.rightOff();
    dry_.init(config.dry);

    const float fadeStep = 1.0f / blocksFor(kFadeMs, config.sampleRate);
    input_.step      = fadeStep;
    wet_.step        = fadeStep;
    digitalDry_.step = fadeStep;
    vca_.step        = fadeStep;
    mute_.step       = 1.0f / blocksFor(kMuteMs, config.sampleRate);
    settleBlocks_    = blocksFor(kSettleMs, config.sampleRate);
    drainBlocks_     = blocksFor(kDrainMs, config.sampleRate);
    updateTargets();
}

void BypassController::setControls(uint16_t runFlags, float blend)
{
    engaged_   = (runFlags & RUN_FLAG_ENGAGED) != 0u;
    trails_    = (runFlags & RUN_FLAG_TRAILS) != 0u;
    analogDry_ = (runFlags & RUN_FLAG_ANALOG_DRY) != 0u;
    stereoIn_  = (runFlags & RUN_FLAG_STEREO_IN) != 0u;
    const bool hasBlend = blend >= 0.0f;
    wetEngaged_ = hasBlend ? blend : 1.0f;
    dryEngaged_ = hasBlend ? 1.0f - blend : 0.0f;
    updateTargets();
}

void BypassController::updateTargets()
{
    // Bypassed, the engine input fades out and its output stays up, so trails ring
    // out. In true bypass the relays take both out of the path.
    const float dry = engaged_ ? dryEngaged_ : 1.0f;
    input_.target      = engaged_ ? 1.0f : 0.0f;
    wet_.target        = engaged_ ? wetEngaged_ : 1.0f;
    vca_.target        = analogDry_ ? dry : 0.0f;
    digitalDry_.target = analogDry_ ? 0.0f : dry;
}

void BypassController::stepRelays()
{
    const bool wantOn = engaged_ || trails_;
    switch (phase_)
    {
        case RelayPhase::Off:
            if (!wantOn) { break; }
            relay_.leftOn();
            relay_.rightOn();
            countdown_ = settleBlocks_;
            phase_ = RelayPhase::Settling;
            break;
        case RelayPhase::Settling:
            if (countdown_ > 0u) { countdown_--; }
            if (countdown_ == 0u) { phase_ = RelayPhase::On; }
            break;
        case RelayPhase::On:
            if (!wantOn) { phase_ = RelayPhase::Releasing; }
            break;
        case RelayPhase::Releasing:
            if (wantOn) { phase_ = RelayPhase::On; }
            else if (mute_.value <= 0.0f)
            {
                countdown_ = drainBlocks_;
                phase_ = RelayPhase::Draining;
            }
            break;
        case RelayPhase::Draining:
            if (countdown_ > 0u) { countdown_--; }
            if (countdown_ == 0u)
            {
                relay_.leftOff();
                relay_.rightOff();
                phase_ = RelayPhase::Off;
            }
            break;
    }
    mute_.target = (phase_ == RelayPhase::On) ? 1.0f : 0.0f;
}

dsp::ConstAudioBuffer BypassController::beginBlock(dsp::ConstAudioBuffer input)
{
    // Mono in: the right input is ignored and the left feeds both sides.
    const float* right = stereoIn_ ? input.right() : input.left();
    dryInput_ = dsp::ConstAudioBuffer(input.left(), right, input.size());

    const uint16_t n = input.size();
    const float start = input_.advance();
    const float delta = (input_.value - start) / n;
    for (uint16_t i = 0; i < n; i++)
    {
        const float gain = start + delta * (i + 1u);
        engineLeft_[i]  = input.leftAt(i) * gain;
        engineRight_[i] = right[i] * gain;
    }
    return dsp::ConstAudioBuffer(engineLeft_, engineRight_, n);
}

void BypassController::endBlock(dsp::AudioBuffer output)
{
    stepRelays();

    const uint16_t n = output.size();
    const float wet0  = wet_.advance();
    const float dry0  = digitalDry_.advance();
    const float mute0 = mute_.advance();
    const float wetD  = (wet_.value - wet0) / n;
    const float dryD  = (digitalDry_.value - dry0) / n;
    const float muteD = (mute_.value - mute0) / n;
    for (uint16_t i = 0; i < n; i++)
    {
        const float k = static_cast<float>(i + 1u);
        const float mute = mute0 + muteD * k;
        const float wet  = wet0 + wetD * k;
        const float dry  = dry0 + dryD * k;
        output.setLeft(i,  mute * (wet * output.leftAt(i)  + dry * dryInput_.leftAt(i)));
        output.setRight(i, mute * (wet * output.rightAt(i) + dry * dryInput_.rightAt(i)));
    }

    // The DAC updates once per block; the VCA follows the same mute as the output.
    (void) vca_.advance();
    const float vca = mute_.value * vca_.value;
    if (vca != vcaWritten_)
    {
        dry_.setGain(vca);
        vcaWritten_ = vca;
    }
}
