#include "gl/game/msvc_random.hpp"

#include <cmath>

namespace gl::game {
MsvcRandom::MsvcRandom(std::uint32_t seed) noexcept : state_(seed) {}

std::uint32_t MsvcRandom::next() noexcept {
    state_ = state_ * 0x343FDu + 0x269EC3u;
    return (state_ >> 16) & 0x7FFFu;
}

double MsvcRandom::fraction() noexcept {
    return static_cast<double>(next()) / 32767.0;
}

std::int32_t MsvcRandom::range(std::int32_t low, std::int32_t high) noexcept {
    if (high <= low)
        return low;
    const double span = static_cast<double>(high - low) + 0.99999;
    const double scaled = span * static_cast<double>(next()) / 32767.0;
    return low + static_cast<std::int32_t>(std::nearbyint(scaled));
}

bool MsvcRandom::coin(double probability) noexcept {
    if (!(probability > 0.0))
        return false;
    if (probability > 1.0)
        return true;
    return fraction() < probability;
}
} // namespace gl::game
