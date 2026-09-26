#include "soft_clipper.hpp"

namespace dsp {

// One curve for the whole block, so the per-sample loop has no branch on it.
template <typename Sat>
static void shapeBlock(const float* input, float* output, uint16_t frames, ParamRamp& drive)
{
    for (uint16_t i = 0; i < frames; i++)
    {
        output[i] = Sat::value(drive.next() * input[i]);
    }
}

void SoftClipper::init(const Config& config)
{
    curve_ = config.curve;
    next_  = config.curve;
    setDrive(config.drive);
    ramp_.snap(drive_);
}

void SoftClipper::setDrive(float drive)
{
    drive_ = clamp(drive, kMinDrive, kMaxDrive);
}

void SoftClipper::process(const float* input, float* output, uint16_t frames)
{
    if (frames == 0u) { return; }
    ramp_.setTarget(drive_, frames);

    if (next_ != curve_)
    {
        crossfade(input, output, frames);
        curve_ = next_;
        return;
    }

    switch (curve_)
    {
    case Curve::Tanh:     shapeBlock<sat::Tanh>(input, output, frames, ramp_);     break;
    case Curve::FastTanh: shapeBlock<sat::FastTanh>(input, output, frames, ramp_); break;
    case Curve::Rational: shapeBlock<sat::Rational>(input, output, frames, ramp_); break;
    case Curve::Sine:     shapeBlock<sat::Sine>(input, output, frames, ramp_);     break;
    case Curve::Cubic:    shapeBlock<sat::Cubic>(input, output, frames, ramp_);    break;
    }
}

// Linear from the old curve to the new across the block, ending on the new.
void SoftClipper::crossfade(const float* input, float* output, uint16_t frames)
{
    const float step = 1.0f / static_cast<float>(frames);
    for (uint16_t i = 0; i < frames; i++)
    {
        const float x   = ramp_.next() * input[i];
        const float old = shape(curve_, x);
        const float mix = static_cast<float>(i + 1u) * step;
        output[i] = old + mix * (shape(next_, x) - old);
    }
}

} // namespace dsp
