#include "gl/script/host.hpp"
#include "gl/assets/cs.hpp"
#include "gl/assets/log.hpp"
#include "gl/script/function_ids.hpp"
#include <string>

namespace gl::script {
namespace {
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

    if (gl::assets::cs_function_is_known(id)) {
        log_stub(gl::LogLevel::debug, id);
        return zero();
    }

    gl::log(gl::LogLevel::warning,
            "script host: unknown function id " + std::to_string(id));
    return zero();
}
} // namespace gl::script
