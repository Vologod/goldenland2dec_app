#include "gl/script/host.hpp"
#include "gl/assets/cs.hpp"
#include "gl/assets/log.hpp"
#include <string>
#include <string_view>

namespace gl::script {
namespace {
constexpr std::uint32_t kFunctionExit = 0x1000000;
constexpr std::uint32_t kFunctionSignal = 0x1000001;
constexpr std::uint32_t kFunctionConsole = 0x1000002;
constexpr std::uint32_t kFunctionCmd = 0x1000003;
constexpr std::uint32_t kFunctionGetPersonParameter = 0x4000000;

Value zero() { return std::int32_t{0}; }

void log_stub(gl::LogLevel level, std::uint32_t id) {
    gl::log(level, std::string("script host: stub function ") +
                       std::string(gl::assets::cs_function_name(id)));
}
} // namespace

Value DefaultScriptHost::call(std::uint32_t id, std::span<const Value>) {
    switch (id) {
    case kFunctionGetPersonParameter:
        return zero();
    case kFunctionExit:
    case kFunctionSignal:
    case kFunctionConsole:
    case kFunctionCmd:
        log_stub(gl::LogLevel::info, id);
        return zero();
    default:
        break;
    }

    const std::string_view name = gl::assets::cs_function_name(id);
    if (name != "unknown") {
        log_stub(gl::LogLevel::debug, id);
        return zero();
    }

    gl::log(gl::LogLevel::warning,
            "script host: unknown function id " + std::to_string(id));
    return zero();
}
} // namespace gl::script
