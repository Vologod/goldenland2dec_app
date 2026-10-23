#pragma once
#include <cstdint>

namespace gl::game {
class MsvcRandom {
  public:
    explicit MsvcRandom(std::uint32_t seed = 1) noexcept;
    std::uint32_t next() noexcept;
    double fraction() noexcept;
    std::int32_t range(std::int32_t low, std::int32_t high) noexcept;
    bool coin(double probability) noexcept;

  private:
    std::uint32_t state_ = 1;
};
} // namespace gl::game
