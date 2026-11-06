#include "gl/game/game_script_host.hpp"
#include "gl/assets/cs.hpp"
#include "gl/assets/log.hpp"
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace gl::game {
GameScriptHost::GameScriptHost(gl::script::NativeServices services)
    : services_(std::move(services)) {}

gl::script::Value GameScriptHost::call(std::uint32_t id,
                                       std::span<const gl::script::Value> args) {
    const auto* info = gl::script::NativeRegistry::find(id);
    if (info == nullptr) {
        gl::log(gl::LogLevel::warning,
                "script host: unknown function id " + std::to_string(id));
        return std::int32_t{0};
    }

    if (info->support == gl::script::NativeSupport::vm_internal) {
        gl::log(gl::LogLevel::warning,
                std::string("script host: vm-internal native reached host: ") +
                    info->name());
        return std::int32_t{0};
    }

    if (info->support == gl::script::NativeSupport::unsupported) {
        if (logged_.insert(id).second) {
            char hex[16];
            std::snprintf(hex, sizeof(hex), "0x%X", id);
            const std::string message =
                std::string("script native ") + info->name() + " (" + hex +
                "): unimplemented — " + std::string(info->reason);
            if (services_.log)
                services_.log(message);
            else
                gl::log(gl::LogLevel::info, message);
        }
        return std::int32_t{0};
    }

    return gl::script::NativeRegistry::call(*info, args, services_);
}
} // namespace gl::game
