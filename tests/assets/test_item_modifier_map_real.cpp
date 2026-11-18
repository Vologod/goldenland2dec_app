#include "gl/assets/hero_save_body.hpp"
#include "gl/assets/item_catalog.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<std::uint8_t> read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

std::vector<std::uint8_t> hero_body(const gl::assets::SavePayload &payload) {
    const auto &block = payload.blocks[6];
    return std::vector<std::uint8_t>(
        payload.decompressed.begin() + static_cast<std::ptrdiff_t>(block.offset),
        payload.decompressed.begin() +
            static_cast<std::ptrdiff_t>(block.offset + block.size));
}

std::uint32_t effect_word3(const gl::assets::ItemEffect &effect) {
    return static_cast<std::uint32_t>(effect.duration.hi) |
           (static_cast<std::uint32_t>(effect.duration.lo) << 8) |
           (static_cast<std::uint32_t>(effect.delay.hi) << 16) |
           (static_cast<std::uint32_t>(effect.delay.lo) << 24);
}
} // namespace

TEST_CASE("real data: ITM effects match saved modifier words" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    std::size_t records = 0;
    std::size_t pairs = 0;
    std::size_t mismatches = 0;
    for (int slot = 0; slot < 28; ++slot) {
        CAPTURE(slot);
        const auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        const auto body = gl::assets::parse_hero_save_body(hero_body(payload.value()));
        REQUIRE(body.has_value());
        for (const auto &container : body->containers) {
            for (const auto &item : container.items) {
                const auto *entry = catalog->find(item.item_id);
                if (entry == nullptr || entry->item.effects.empty())
                    continue;
                ++records;
                if (entry->item.effects.size() > item.modifiers.size()) {
                    ++mismatches;
                    continue;
                }
                for (std::size_t index = 0; index < entry->item.effects.size(); ++index) {
                    const auto &effect = entry->item.effects[index];
                    const auto &words = item.modifiers[index].words;
                    if (words[0] != effect.type || words[1] != effect.amount ||
                        words[2] != effect.flags || words[3] != effect_word3(effect) ||
                        words[4] != 0xFFFFFFFFu) {
                        ++mismatches;
                        continue;
                    }
                    ++pairs;
                }
            }
        }
    }
    CHECK(records == 250);
    CHECK(pairs == 741);
    CHECK(mismatches == 0);
}
