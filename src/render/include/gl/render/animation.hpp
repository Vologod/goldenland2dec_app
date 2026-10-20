#pragma once

#include <cstddef>
#include <cstdint>

namespace gl::render {

std::size_t animation_frame_index(std::uint64_t elapsed_ms, std::uint32_t delay_ms,
                                  std::size_t frame_count);

}
