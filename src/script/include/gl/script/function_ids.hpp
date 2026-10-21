#pragma once
#include <cstdint>

namespace gl::script {
inline constexpr std::uint32_t kFunctionExit = 0x1000000;
inline constexpr std::uint32_t kFunctionSignal = 0x1000001;
inline constexpr std::uint32_t kFunctionConsole = 0x1000002;
inline constexpr std::uint32_t kFunctionCmd = 0x1000003;
inline constexpr std::uint32_t kFunctionSay = 0x1000004;
inline constexpr std::uint32_t kFunctionCloseDialog = 0x1000005;
inline constexpr std::uint32_t kFunctionAnswer = 0x1000006;
inline constexpr std::uint32_t kFunctionGetPersonParameter = 0x4000000;
} // namespace gl::script
