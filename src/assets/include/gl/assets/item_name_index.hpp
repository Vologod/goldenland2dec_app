#pragma once
#include "gl/assets/result.hpp"
#include "gl/assets/sdb.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace gl::assets {
class GameVfs;

class ItemNameIndex {
  public:
    static Result<ItemNameIndex> load(const GameVfs &vfs);
    static Result<ItemNameIndex> from_entries(std::span<const SdbEntry> entries);
    std::optional<std::uint32_t> find(std::string_view tech_name) const;

  private:
    std::map<std::string, std::uint32_t, std::less<>> by_name_;
};
} // namespace gl::assets
