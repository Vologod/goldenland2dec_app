#pragma once
#include "gl/assets/result.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gl::assets {
struct CsNode {
    std::int32_t opcode = -1;
    std::int32_t a = -1, b = -1, c = -1, d = -1;
    std::array<std::int32_t, 9> args = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    std::string text;
    double value = -1.0;
};

Result<std::vector<CsNode>> parse_cs(std::span<const std::uint8_t> data);
const char* cs_opcode_name(std::int32_t opcode);
const char* cs_function_name(std::uint32_t id);
bool cs_function_is_known(std::uint32_t id);
bool cs_opcode_valid(std::int32_t opcode);
}
