#include "gl/game/trigger_runtime.hpp"

namespace gl::game {
void TriggerRuntime::reset(std::span<const gl::assets::LvlDescription> triggers) {
    triggers_.clear();
    for (const gl::assets::LvlDescription &trigger : triggers)
        triggers_.emplace(trigger.name, false);
}

bool TriggerRuntime::set(std::string_view name, bool enabled) {
    const auto it = triggers_.find(std::string(name));
    if (it == triggers_.end())
        return false;
    if (enabled)
        it->second = true;
    return true;
}

bool TriggerRuntime::enabled(std::string_view name) const {
    const auto it = triggers_.find(std::string(name));
    return it != triggers_.end() && it->second;
}

gl::script::TriggerAccess TriggerRuntime::access() {
    gl::script::TriggerAccess access;
    access.set = [this](std::string_view name, bool enabled) { return set(name, enabled); };
    return access;
}
} // namespace gl::game
