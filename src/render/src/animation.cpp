#include "gl/render/animation.hpp"

namespace gl::render {

std::size_t animation_frame_index(std::uint64_t elapsed_ms, std::uint32_t delay_ms,
                                  std::size_t frame_count) {
    if (delay_ms == 0 || frame_count == 0) return 0;
    return static_cast<std::size_t>((elapsed_ms / delay_ms) % frame_count);
}

}
