#include "gl/game/msvc_random.hpp"

#include <chrono>

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
    const double scaled = span * static_cast<double>(next()) * (1.0 / 32767.0);
    return low + static_cast<std::int32_t>(scaled);
}

bool MsvcRandom::coin(double probability) noexcept {
    const float p = static_cast<float>(probability);
    if (p <= 0.0f)
        return false;
    if (p > 1.0f)
        return true;
    return static_cast<float>(next()) * (1.0f / 32767.0f) < p;
}

std::uint32_t MsvcRandom::production_seed() noexcept {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    return static_cast<std::uint32_t>(ms);
}
} // namespace gl::game
