#pragma once
#include <functional>
#include <string_view>

namespace gl::script {
struct GlobalAccess {
    std::function<void(std::string_view, double)> set_double;
};
} // namespace gl::script
