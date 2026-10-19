#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
namespace gl::assets {
std::string cp1251_to_utf8(std::span<const std::uint8_t> input);
std::string normalize_virtual_path(std::string_view path);
bool is_safe_relative_path(std::string_view path);
}
