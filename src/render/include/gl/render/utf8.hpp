#pragma once

#include <string_view>
#include <vector>

namespace gl::render {

std::vector<char32_t> utf8_decode(std::string_view utf8);

}
