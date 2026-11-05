#pragma once
#include "gl/assets/itm.hpp"
#include "gl/assets/result.hpp"
#include <cstdint>
#include <map>
#include <string>

namespace gl::assets {
class GameVfs;

struct ItemCatalogEntry {
    Item item;
    std::string name;
    std::string description;
    std::string icon_path;
};

class ItemCatalog {
  public:
    static Result<ItemCatalog> load(const GameVfs &vfs);
    const ItemCatalogEntry *find(std::uint32_t item_id) const;

  private:
    const GameVfs *vfs_ = nullptr;
    std::map<std::uint32_t, std::string> names_;
    std::map<std::uint32_t, std::string> descriptions_;
    mutable std::map<std::uint32_t, ItemCatalogEntry> cache_;
};
} // namespace gl::assets
