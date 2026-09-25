#pragma once
#include <cstdint>

namespace dsp {

// Linear ramp from the current value to a new target across one block, as stmlib's
// ParameterInterpolator. Control sets arrive as steps (pots every 2 ms, MIDI CCs in
// 1/128 of the range), which a level, feedback or mix param would otherwise make
// audible as zipper noise. Not for time or pitch params, which need their own glide,
// or for quantised params.
class ParamRamp {
public:

    // Jumps to value: at activation, or when BlockContext::first is set.
    void snap(float value)
    {
        value_     = value;
        increment_ = 0.0f;
    }

    // Once per block, before the sample loop. Repeating it with the same target
    // changes nothing, so it can run every block.
    void setTarget(float target, uint16_t frames)
    {
        increment_ = (target - value_) / static_cast<float>(frames);
    }

    // Once per sample; after `frames` calls the value is at the target.
    float next()
    {
        value_ += increment_;
        return value_;
    }

    float value() const { return value_; }

private:
    float value_     = 0.0f;
    float increment_ = 0.0f;
};

} // namespace dsp
