#pragma once
#include <cstdint>
#include <functional>
#include <string_view>

namespace gl::script {
struct PartyAccess {
    std::function<std::int32_t(std::string_view)> add;
    std::function<std::int32_t(std::string_view)> remove;
    std::function<bool(std::string_view)> has;
};
} // namespace gl::script
