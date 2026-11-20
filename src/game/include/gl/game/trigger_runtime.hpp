#pragma once
#include "gl/assets/lvl.hpp"
#include "gl/script/trigger_access.hpp"

#include <cstddef>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace gl::game {
class TriggerRuntime {
  public:
    void reset(std::span<const gl::assets::LvlDescription> triggers);
    bool set(std::string_view name, bool enabled);
    bool enabled(std::string_view name) const;
    std::size_t size() const { return triggers_.size(); }
    void clear() { triggers_.clear(); }

    gl::script::TriggerAccess access();

  private:
    std::map<std::string, bool> triggers_;
};
} // namespace gl::game
