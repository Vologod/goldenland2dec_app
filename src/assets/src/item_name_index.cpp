#include "gl/assets/item_name_index.hpp"
#include "gl/assets/log.hpp"
#include "gl/assets/vfs.hpp"

#include <string>

namespace gl::assets {
namespace {
constexpr std::string_view kTechNamesPath = "sdb/items/tech_names.sdb";
} // namespace

Result<ItemNameIndex> ItemNameIndex::load(const GameVfs &vfs) {
    auto bytes = vfs.read(kTechNamesPath);
    if (!bytes)
        return Error{ErrorCode::io, "item name index: cannot read sdb/items/tech_names.sdb"};
    auto sdb = Sdb::parse(bytes.value());
    if (!sdb)
        return sdb.error();
    return from_entries(sdb->entries());
}

Result<ItemNameIndex> ItemNameIndex::from_entries(std::span<const SdbEntry> entries) {
    ItemNameIndex index;
    for (const auto &entry : entries) {
        const auto inserted = index.by_name_.emplace(entry.value, entry.id);
        if (!inserted.second)
            log(LogLevel::warning,
                "item name index: duplicate tech name '" + entry.value + "'");
    }
    return index;
}

std::optional<std::uint32_t> ItemNameIndex::find(std::string_view tech_name) const {
    const auto found = by_name_.find(tech_name);
    if (found == by_name_.end())
        return std::nullopt;
    return found->second;
}
} // namespace gl::assets
