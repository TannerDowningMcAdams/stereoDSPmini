#pragma once
#include <array>
#include <cstdint>

namespace dsp {

// Wright omega, w(x) = W(e^x) for the Lambert W function: the w solving
// w + ln(w) = x. Taking the exponent as the argument keeps diode equations in range,
// where e^x itself would overflow.
//
// Read from two tables of cubic Hermite knots filled at compile time
// (wright_omega.cpp), each knot carrying w and its slope w / (1 + w). Fine knots
// cover -20..16, where the curve bends and small values need relative precision;
// coarse knots cover 16..656, where it is nearly x - ln(x). Relative error below
// 2e-6. Below -20, where w < 2.1e-9, it gives 0, as does NaN; past 656 it continues
// along the last slope.
struct OmegaKnot
{
    float value;
    float step;     // slope times the knot spacing
};

namespace omega_table {

constexpr float    kFineLo      = -20.0f;
constexpr float    kFineStep    = 0.125f;
constexpr uint32_t kFineCount   = 289;
constexpr float    kCoarseLo    = 16.0f;
constexpr float    kCoarseStep  = 2.0f;
constexpr uint32_t kCoarseCount = 321;

extern const std::array<OmegaKnot, kFineCount>   kFine;
extern const std::array<OmegaKnot, kCoarseCount> kCoarse;

// Between knot k[0] and k[1], t in [0, 1].
inline float hermite(const OmegaKnot* k, float t)
{
    const float d0 = k[0].step;
    const float d1 = k[1].step;
    const float dw = k[1].value - k[0].value;
    return k[0].value + t * (d0 + t * (3.0f * dw - 2.0f * d0 - d1 + t * (d0 + d1 - 2.0f * dw)));
}

} // namespace omega_table

inline float wrightOmega(float x)
{
    using namespace omega_table;
    if (!(x > kFineLo)) { return 0.0f; }
    if (x < kCoarseLo)
    {
        const float f = (x - kFineLo) * (1.0f / kFineStep);
        uint32_t    i = static_cast<uint32_t>(f);
        // Just under kCoarseLo, f can round up to the last knot.
        if (i > kFineCount - 2u) { i = kFineCount - 2u; }
        return hermite(&kFine[i], f - static_cast<float>(i));
    }
    const float f    = (x - kCoarseLo) * (1.0f / kCoarseStep);
    const float last = static_cast<float>(kCoarseCount - 1u);
    if (f >= last) { return kCoarse[kCoarseCount - 1u].value + (f - last) * kCoarse[kCoarseCount - 1u].step; }
    const uint32_t i = static_cast<uint32_t>(f);
    return hermite(&kCoarse[i], f - static_cast<float>(i));
}

} // namespace dsp
