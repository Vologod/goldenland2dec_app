#include "gl/assets/csx.hpp"
#include "gl/assets/iad.hpp"
#include "gl/assets/itm.hpp"
#include "gl/assets/pak.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <doctest/doctest.h>
#include <iostream>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::string lower_extension(std::string_view name) {
    const std::size_t slash = name.find_last_of("/\\");
    const std::size_t dot = name.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash))
        return {};
    std::string ext(name.substr(dot));
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}
} // namespace

TEST_CASE("real data: archive counts and samples" * doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto count_for = [&](std::string_view name) -> std::size_t {
        for (const auto &a : vfs->archives())
            if (a.name == name)
                return a.entry_count;
        return 0;
    };
    CHECK(count_for("scripts.pak") == 4485);
    CHECK(count_for("sdb.pak") == 45);
    CHECK(count_for("items.pak") == 3786);
    CHECK(count_for("engineres.pak") == 1041);

    auto hero = vfs->read("sdb/hero.sdb");
    REQUIRE(hero.has_value());
    auto sdb = Sdb::parse(hero.value());
    CHECK(sdb.has_value());

    auto item = vfs->read("items/data/0.itm");
    REQUIRE(item.has_value());
    auto parsed = parse_item(item.value());
    CHECK(parsed.has_value());
}

TEST_CASE("real data: every non-empty CSX, SDB and ITM entry decodes" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    enum Kind { kCsx, kSdb, kItm, kKindCount };
    std::array<std::size_t, kKindCount> decoded{};
    std::array<std::size_t, kKindCount> empty{};
    std::size_t failures = 0;

    for (const auto& info : vfs->archives()) {
        const PakArchive* archive = vfs->archive(info.name);
        REQUIRE(archive != nullptr);
        for (const auto& entry : archive->entries()) {
            const std::string ext = lower_extension(entry.name);
            Kind kind = kKindCount;
            if (ext == ".csx") kind = kCsx;
            else if (ext == ".sdb") kind = kSdb;
            else if (ext == ".itm") kind = kItm;
            if (kind == kKindCount) continue;

            auto data = archive->read(entry);
            if (!data) {
                ++failures;
                std::cout << "read failure: " << info.name << ':' << entry.name << ": "
                          << data.error().message << '\n';
                continue;
            }
            if (data.value().empty()) {
                ++empty[static_cast<std::size_t>(kind)];
                continue;
            }
            bool ok = false;
            std::string reason;
            if (kind == kCsx) {
                auto result = decode_csx(data.value());
                ok = result.has_value();
                if (!ok) reason = result.error().message;
            } else if (kind == kSdb) {
                auto result = Sdb::parse(data.value());
                ok = result.has_value();
                if (!ok) reason = result.error().message;
            } else {
                auto result = parse_item(data.value());
                ok = result.has_value();
                if (!ok) reason = result.error().message;
            }
            if (ok) {
                ++decoded[static_cast<std::size_t>(kind)];
            } else {
                ++failures;
                std::cout << "decode failure: " << info.name << ':' << entry.name << ": " << reason
                          << '\n';
            }
        }
    }

    std::cout << "real data decoded: .csx=" << decoded[kCsx] << " .sdb=" << decoded[kSdb]
              << " .itm=" << decoded[kItm] << '\n';
    std::cout << "real data empty: .csx=" << empty[kCsx] << " .sdb=" << empty[kSdb]
              << " .itm=" << empty[kItm] << '\n';
    CHECK(failures == 0);
    CHECK(decoded[kCsx] > 0);
    CHECK(decoded[kSdb] > 0);
    CHECK(decoded[kItm] > 0);
}
TEST_CASE("real data: dospeh_kamen.iad parses into blocks" *
          doctest::skip(!game_dir_available())) {
    const char* dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto data = vfs->read("wear/noweapon_thrw/armor/dospeh_kamen/dospeh_kamen.iad");
    REQUIRE(data.has_value());
    auto blocks = parse_iad(data.value());
    REQUIRE(blocks.has_value());
    REQUIRE(blocks->size() >= 3);
    CHECK(blocks->front().type == 1);
    CHECK(blocks->front().frames == 12);
    CHECK(blocks->front().frame_width == 32);
    CHECK(blocks->front().frame_height == 38);
    CHECK(blocks->front().crops.size() == 60);
    CHECK(blocks->at(1).type == 4);
    CHECK(blocks->at(1).frame_width == 36);
    CHECK(blocks->at(1).crops.size() == 60);
}

TEST_CASE("real data: slot0 outfit item IDs map to wear assets" *
          doctest::skip(!game_dir_available())) {
    const char* dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    struct Expected {
        const char *id;
        ItemType type;
        const char *puppet_part;
        const char *wear_path;
    };
    const Expected expected[] = {
        {"318", ItemType::cuirass, "Almaz", "wear/noweapon_thrw/armor/dospeh_almaz/animation/tb_stay.csx"},
        {"397", ItemType::leggings, "Goluboi", "wear/noweapon_thrw/elbarms/ponogi_goluboy/animation/tb_stay.csx"},
        {"280", ItemType::helmet, "Cherny", "wear/noweapon_thrw/helmet/shlem_cherny/animation/tb_stay.csx"},
    };
    for (const Expected &entry : expected) {
        auto raw = vfs->read(std::string("items/data/") + entry.id + ".itm");
        REQUIRE(raw.has_value());
        auto item = parse_item(raw.value());
        REQUIRE(item.has_value());
        CHECK(item->type == entry.type);
        CHECK(item->puppet_name.find(entry.puppet_part) != std::string::npos);
        if (std::string(entry.id) == "280") {
            CHECK_FALSE(vfs->read(entry.wear_path).has_value());
        } else {
            CHECK(vfs->read(entry.wear_path).has_value());
        }
    }
}
