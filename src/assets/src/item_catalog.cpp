#include "gl/assets/item_catalog.hpp"
#include "gl/assets/cp1251.hpp"
#include "gl/assets/log.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"

#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace gl::assets {
namespace {
constexpr std::string_view kLitNamesPath = "sdb/items/lit_names.sdb";
constexpr std::string_view kDescriptionsPath = "sdb/items/descriptions.sdb";
constexpr std::string_view kFallbackIcon = "engineres/interface/bag";

void load_texts(const GameVfs &vfs, std::string_view path,
                std::map<std::uint32_t, std::string> &out) {
    auto bytes = vfs.read(path);
    if (!bytes) {
        log(LogLevel::warning, "item catalog: cannot read " + std::string(path));
        return;
    }
    auto sdb = Sdb::parse(bytes.value());
    if (!sdb) {
        log(LogLevel::warning, "item catalog: cannot parse " + std::string(path));
        return;
    }
    for (const auto &entry : sdb->entries())
        out.emplace(entry.id, entry.value);
}

const std::string *lookup(const std::map<std::uint32_t, std::string> &values, std::uint32_t id) {
    const auto found = values.find(id);
    return found == values.end() ? nullptr : &found->second;
}
} // namespace

Result<ItemCatalog> ItemCatalog::load(const GameVfs &vfs) {
    ItemCatalog catalog;
    catalog.vfs_ = &vfs;
    load_texts(vfs, kLitNamesPath, catalog.names_);
    load_texts(vfs, kDescriptionsPath, catalog.descriptions_);
    return catalog;
}

const ItemCatalogEntry *ItemCatalog::find(std::uint32_t item_id) const {
    const auto cached = cache_.find(item_id);
    if (cached != cache_.end())
        return &cached->second;

    ItemCatalogEntry entry;
    entry.icon_path = std::string(kFallbackIcon);
    bool found = false;
    if (vfs_ != nullptr) {
        auto raw = vfs_->read("items/data/" + std::to_string(item_id) + ".itm");
        if (raw) {
            auto item = parse_item(raw.value());
            if (item) {
                entry.item = std::move(item.value());
                found = true;

                if (const std::string *name = lookup(names_, entry.item.description_id);
                    name != nullptr && !name->empty())
                    entry.name = *name;
                else
                    entry.name = "item " + std::to_string(item_id);

                if (const std::string *description =
                        lookup(descriptions_, entry.item.description_id);
                    description != nullptr)
                    entry.description = *description;

                if (!entry.item.icon_name.empty()) {
                    std::string path = "items/res/" + normalize_virtual_path(entry.item.icon_name);
                    if (vfs_->exists(path))
                        entry.icon_path = std::move(path);
                }
            }
        }
    }
    if (!found)
        entry.name = "item " + std::to_string(item_id);

    const auto inserted = cache_.emplace(item_id, std::move(entry));
    return &inserted.first->second;
}
} // namespace gl::assets
