#pragma once
#include <bit>
#include <cmath>
#include <cstdint>

namespace dsp {

constexpr float kPi     = 3.14159265f;
constexpr float kTwoPi  = 6.28318531f;
constexpr float kHalfPi = 1.57079633f;

// Limits x to [lo, hi]. NaN goes to lo, so a bad control value cannot reach a
// coefficient.
inline float clamp(float x, float lo, float hi)
{
    if (!(x > lo)) { return lo; }
    if (x > hi)    { return hi; }
    return x;
}

// sin(pi/2 * t) for t in [-1, 1]. Taylor series to t^9, with the last term trimmed
// so sinHalfPi(1) is exactly 1. Error below 4e-6; just short of +-1, rounding can
// put it one float step (1.2e-7) beyond +-1.
inline float sinHalfPi(float t)
{
    const float t2 = t * t;
    return t * (1.57079633f + t2 * (-0.645964098f + t2 * (0.0796926262f +
           t2 * (-0.00468175413f + t2 * 0.000156895900f))));
}

// The clip curves below share a shape: unity slope at zero, rising to +-1, and held
// there past the knee. This is the held part: +-1, and 0 for NaN.
inline float rail(float x)
{
    return (x != x) ? 0.0f : std::copysign(1.0f, x);
}

// Rational tanh approximation, x (27 + x^2) / (27 + 9 x^2): exactly +-1 with zero
// slope from the knee at |x| = 3. Within 0.024 of tanh.
inline float rationalClip(float x)
{
    if (!(x > -3.0f) || !(x < 3.0f)) { return rail(x); }
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// sin(x) up to the knee at |x| = pi/2: zero slope there, so the knee is smooth
// but reached sooner than the rational curve's.
inline float sineClip(float x)
{
    if (!(x > -kHalfPi) || !(x < kHalfPi)) { return rail(x); }
    return sinHalfPi(x * (1.0f / kHalfPi));
}

// x - (4/27) x^3 up to the knee at |x| = 1.5, where it reaches +-1 with zero
// slope: the classic cubic clipper scaled to unity slope at zero. The hardest knee
// of the smooth curves, and no division.
inline float cubicClip(float x)
{
    if (!(x > -1.5f) || !(x < 1.5f)) { return rail(x); }
    return x * (1.0f - (4.0f / 27.0f) * x * x);
}

// tanh to within 4e-6, odd, monotone, and exactly +-1 from |x| = 9. NaN gives 0.
// Computed as 1 - 2 / (e^2|x| + 1), with e^2|x| = 2^(2|x| log2 e): the whole part
// goes into the exponent bits and a degree-5 polynomial covers the fraction. The
// polynomial's last coefficient is set so the pieces meet at every whole number;
// a jump there would read as a spike in the slope. Below 1/8 that form loses
// relative precision to cancellation, so a Taylor series takes over.
inline float tanhFast(float x)
{
    const float a = std::fabs(x);
    if (!(a < 9.0f)) { return rail(x); }
    if (a < 0.125f)
    {
        const float x2 = x * x;
        return x * (1.0f + x2 * (-0.333333333f + x2 * 0.133333333f));
    }

    const float    y     = a * 2.88539008f;             // 2|x| log2(e), in [0.36, 26)
    const uint32_t whole = static_cast<uint32_t>(y);
    const float    r     = y - static_cast<float>(whole);
    const float    frac  = 1.0f + r * (0.693147182f + r * (0.240226507f + r * (0.0555041087f +
                           r * (0.00961812911f + r * 0.0015040732f))));
    const float    e     = std::bit_cast<float>(std::bit_cast<uint32_t>(frac) + (whole << 23));

    return std::copysign(1.0f - 2.0f / (e + 1.0f), x);
}

// Saturating curves, passed as template arguments to the nonlinear filters and
// SoftClipper. All have unity slope at zero and saturate at +-1, so a given drive
// means the same small-signal gain on each. value(x) is the curve. slope(x, y) is
// the derivative of that same curve at x, given y = value(x), for a solver that
// needs one: a Jacobian taken from a curve other than the one evaluated slows
// Newton's method or makes it diverge.
namespace sat {

// std::tanh, with NaN giving 0 as the other curves do. The reference; the slowest.
struct Tanh
{
    static float value(float x)          { return (x != x) ? 0.0f : std::tanh(x); }
    static float slope(float, float y)   { return 1.0f - y * y; }
};

// tanhFast: within 4e-6 of tanh, so 1 - y^2 is its own slope to within 1e-7.
struct FastTanh
{
    static float value(float x)          { return tanhFast(x); }
    static float slope(float, float y)   { return 1.0f - y * y; }
};

// rationalClip: a harder knee than tanh. 1 - y^2 is up to 0.05 off its slope, so
// the slope is its own.
struct Rational
{
    static float value(float x)          { return rationalClip(x); }
    static float slope(float x, float)
    {
        if (!(x > -3.0f) || !(x < 3.0f)) { return 0.0f; }
        const float x2  = x * x;
        const float n   = 9.0f - x2;
        const float den = 27.0f + 9.0f * x2;
        return 9.0f * n * n / (den * den);
    }
};

// sineClip. The slope is cos(x), taken as sin(pi/2 - |x|) with the same polynomial.
struct Sine
{
    static float value(float x)          { return sineClip(x); }
    static float slope(float x, float)
    {
        if (!(x > -kHalfPi) || !(x < kHalfPi)) { return 0.0f; }
        return sinHalfPi(1.0f - std::fabs(x) * (1.0f / kHalfPi));
    }
};

// cubicClip. The cheapest curve.
struct Cubic
{
    static float value(float x)          { return cubicClip(x); }
    static float slope(float x, float)
    {
        if (!(x > -1.5f) || !(x < 1.5f)) { return 0.0f; }
        return 1.0f - (4.0f / 9.0f) * x * x;
    }
};

} // namespace sat

} // namespace dsp
