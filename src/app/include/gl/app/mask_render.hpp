#pragma once

#include <gl/assets/image.hpp>

namespace gl::app {

gl::assets::Image compose_mask_foreground(const gl::assets::Image& background,
                                          const gl::assets::Image& mask, int x, int y);

}
