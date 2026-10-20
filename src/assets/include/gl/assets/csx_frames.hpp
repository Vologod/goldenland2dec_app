#pragma once
#include "gl/assets/csx.hpp"
#include "gl/assets/image.hpp"
#include "gl/assets/result.hpp"
#include <cstdint>
#include <vector>

namespace gl::assets {
Result<std::vector<Image>> slice_csx_frames(const CsxImage& image, std::uint32_t frame_height);
}
