#pragma once
#include "gl/script/party_access.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gl::game {
class PartyRuntime {
  public:
    std::int32_t add(std::string_view name);
    std::int32_t remove(std::string_view name);
    bool has(std::string_view name) const;
    std::size_t size() const { return members_.size(); }
    void clear() { members_.clear(); }

    gl::script::PartyAccess access();

  private:
    static constexpr std::size_t kMaxParty = 4;
    std::vector<std::string> members_;
};
} // namespace gl::game
