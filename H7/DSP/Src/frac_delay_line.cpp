#include "frac_delay_line.hpp"

namespace dsp {

void FracDelayLine::init(const Config& config)
{
    uint32_t size = 1u;
    while (size <= config.size / 2u) { size *= 2u; }

    buffer_   = config.buffer;
    mask_     = size - 1u;
    maxDelay_ = (size > kGuard) ? static_cast<float>(size - kGuard) : 0.0f;
    reset();
}

void FracDelayLine::reset()
{
    for (uint32_t i = 0; i <= mask_; i++)
    {
        buffer_[i] = 0.0f;
    }
    write_ = 0;
}

} // namespace dsp
