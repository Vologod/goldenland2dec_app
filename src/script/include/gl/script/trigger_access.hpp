#pragma once
#include <functional>
#include <string_view>

namespace gl::script {
struct TriggerAccess {
    std::function<bool(std::string_view, bool)> set;
};
} // namespace gl::script
