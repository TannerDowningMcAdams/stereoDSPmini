#include "wright_omega.hpp"

namespace dsp {
namespace omega_table {

// std::exp and std::log are not constexpr, so the table builds on its own.
namespace {

constexpr double kLn2 = 0.693147180559945309;

// |x| below about 700. Reduced to r in [-ln2/2, ln2/2], where 16 Taylor terms
// reach double precision.
constexpr double cexp(double x)
{
    const int    k = static_cast<int>(x / kLn2 + ((x < 0.0) ? -0.5 : 0.5));
    const double r = x - k * kLn2;
    double term = 1.0, sum = 1.0;
    for (int n = 1; n < 16; n++) { term *= r / n; sum += term; }
    for (int n = 0; n < k; n++) { sum *= 2.0; }
    for (int n = 0; n > k; n--) { sum *= 0.5; }
    return sum;
}

// x > 0. The mantissa m in [1, 2) goes through ln(m) = 2 atanh((m - 1) / (m + 1)).
constexpr double clog(double x)
{
    double e = 0.0;
    while (x >= 2.0) { x *= 0.5; e += 1.0; }
    while (x < 1.0)  { x *= 2.0; e -= 1.0; }
    const double z  = (x - 1.0) / (x + 1.0);
    const double z2 = z * z;
    double term = z, sum = 0.0;
    for (int n = 1; n < 40; n += 2) { sum += term / n; term *= z2; }
    return 2.0 * sum + e * kLn2;
}

// Newton on t = ln(w), solving t + e^t = x. The function is convex and rising, so
// from a start above the root the steps fall onto it without overshoot.
constexpr double omega(double x)
{
    double t = (x > 1.0) ? clog(x) : x;
    for (int k = 0; k < 10; k++) {
        const double e = cexp(t);
        t -= (t + e - x) / (1.0 + e);
    }
    return cexp(t);
}

template <uint32_t N>
consteval std::array<OmegaKnot, N> makeKnots(double lo, double step)
{
    std::array<OmegaKnot, N> knots{};
    for (uint32_t i = 0; i < N; i++) {
        const double w = omega(lo + step * i);
        knots[i] = {static_cast<float>(w), static_cast<float>(step * w / (1.0 + w))};
    }
    return knots;
}

} // namespace

constinit const std::array<OmegaKnot, kFineCount>   kFine   = makeKnots<kFineCount>(kFineLo, kFineStep);
constinit const std::array<OmegaKnot, kCoarseCount> kCoarse = makeKnots<kCoarseCount>(kCoarseLo, kCoarseStep);

} // namespace omega_table
} // namespace dsp
