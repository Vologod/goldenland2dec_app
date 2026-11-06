#pragma once
#include "gl/script/host.hpp"
#include "gl/script/native_registry.hpp"
#include <cstdint>
#include <set>
#include <span>

namespace gl::game {
class GameScriptHost : public gl::script::ScriptHost {
  public:
    explicit GameScriptHost(gl::script::NativeServices services = {});
    gl::script::Value call(std::uint32_t id,
                           std::span<const gl::script::Value> args) override;
    bool stop_requested() const override;
    gl::script::NativeServices& services() { return services_; }

  private:
    gl::script::NativeServices services_;
    std::set<std::uint32_t> logged_;
    bool stop_requested_ = false;
};
} // namespace gl::game
