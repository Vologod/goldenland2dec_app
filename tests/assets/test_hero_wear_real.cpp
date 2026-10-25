#include "gl/assets/iad.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/assets/wear_had.hpp"
#include "gl/assets/wear_seq.hpp"
#include "gl/game/hero_wear.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::game::HeroOutfit slot0() {
    gl::game::HeroOutfit o;
    o.wear_set = "noweapon_thrw";
    o.armor = "dospeh_almaz";
    o.elbarms = "ponogi_goluboy";
    o.helmet = "shlem_goluboy";
    return o;
}
} // namespace

TEST_CASE("real wear: HAD noweapon_thrw has engine values" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto data = vfs->read("wear/noweapon_thrw/noweapon_thrw.had");
    REQUIRE(data.has_value());
    auto had = gl::assets::parse_wear_had(data.value());
    REQUIRE(had.has_value());
    CHECK(had->size() == 11);
    const gl::assets::WearHadAnimation *tb_stay = nullptr;
    for (const auto &a : had.value())
        if (a.type_mask == 0x4)
            tb_stay = &a;
    REQUIRE(tb_stay != nullptr);
    CHECK(tb_stay->delay_ms == 88);
    CHECK(tb_stay->columns == 12);
    CHECK(tb_stay->frame_width == 48);
    CHECK(tb_stay->frame_height == 92);
    CHECK(tb_stay->anchor_x == 24);
    CHECK(tb_stay->anchor_y == 77);
    CHECK(tb_stay->layer_offset_x == 158);
    CHECK(tb_stay->layer_offset_y == 142);
    CHECK(tb_stay->rows == 5);

    const gl::assets::WearHadAnimation *tb_go = nullptr;
    for (const auto &a : had.value())
        if (a.type_mask == 0x10)
            tb_go = &a;
    REQUIRE(tb_go != nullptr);
    CHECK(tb_go->rows == 9);
    CHECK(tb_go->delay_ms == 82);
    CHECK(tb_go->frame_width == 65);
    CHECK(tb_go->frame_height == 95);
    CHECK(tb_go->movement_x == doctest::Approx(5.833333f));
    CHECK(tb_go->movement_y == doctest::Approx(5.0f));
}

TEST_CASE("real wear: IAD armor and seq parse" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto iad_data = vfs->read("wear/noweapon_thrw/armor/dospeh_almaz/dospeh_almaz.iad");
    REQUIRE(iad_data.has_value());
    auto blocks = gl::assets::parse_iad(iad_data.value());
    REQUIRE(blocks.has_value());
    const gl::assets::IadBlock *type4 = nullptr;
    for (const auto &b : blocks.value())
        if (b.type == 0x4)
            type4 = &b;
    REQUIRE(type4 != nullptr);
    CHECK(type4->frames == 12);
    CHECK(type4->frame_width == 36);
    CHECK(type4->frame_height == 37);
    CHECK(type4->layer_offset_x == 165);
    CHECK(type4->layer_offset_y == 154);
    REQUIRE(type4->crops.size() == 60);
    CHECK(type4->crops[0].x1 == 1);
    CHECK(type4->crops[0].y1 == 5);
    CHECK(type4->shadow_frame_width == 40);
    CHECK(type4->shadow_frame_height == 20);
    CHECK(type4->shadow_layer_offset_x == 152);
    CHECK(type4->shadow_layer_offset_y == 205);
    REQUIRE(type4->shadow_crops.size() == 96);

    auto seq_data = vfs->read("wear/noweapon_thrw/seq/tb_stay_armor.seq");
    REQUIRE(seq_data.has_value());
    auto seq = gl::assets::parse_wear_seq(seq_data.value());
    REQUIRE(seq.has_value());
    CHECK(seq->stride == 12);
    CHECK(seq->rows == 5);
    for (std::uint8_t v : seq->order)
        CHECK(v == 2);
}

TEST_CASE("real wear: slot0 visual composes expected sheets" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto visual = gl::game::load_hero_visual(vfs.value(), slot0());
    REQUIRE(visual.has_value());
    const auto *idle = visual->find(0x4);
    REQUIRE(idle != nullptr);
    CHECK(idle->body.width == 576);
    CHECK(idle->body.height == 460);
    CHECK(idle->shadow.width == 660);
    CHECK(idle->shadow.height == 256);
    const auto *go = visual->find(0x10);
    REQUIRE(go != nullptr);
    CHECK(go->body.width == 780);
    CHECK(go->body.height == 855);
    const auto any_alpha = [&](int x, int y, int w, int h) {
        for (int yy = y; yy < y + h; ++yy) {
            for (int xx = x; xx < x + w; ++xx) {
                if (idle->body.rgba[static_cast<std::size_t>(yy) * idle->body.width + xx].a != 0)
                    return true;
            }
        }
        return false;
    };
    CHECK(any_alpha(8, 17, 31, 31));
    CHECK(any_alpha(9, 46, 26, 42));
    CHECK(any_alpha(18, 3, 12, 20));
}

TEST_CASE("real wear: slot0 idle sheet matches golden hash" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto visual = gl::game::load_hero_visual(vfs.value(), slot0());
    REQUIRE(visual.has_value());
    const auto *idle = visual->find(0x4);
    REQUIRE(idle != nullptr);
    std::uint64_t hash = 1469598103934665603ull;
    for (const auto &p : idle->body.rgba) {
        hash ^= (static_cast<std::uint64_t>(p.r) << 24) | (static_cast<std::uint64_t>(p.g) << 16) |
                (static_cast<std::uint64_t>(p.b) << 8) | p.a;
        hash *= 1099511628211ull;
    }
    CHECK(hash == 0x1A0C2E25C84A1174ull);
}

TEST_CASE("real wear: all sets parse HAD/IAD/SEQ" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const char *sets[] = {"bows",           "noweapon_thrw",     "onehand_wpn",
                          "two_axes_maces", "two_spears_staffs", "two_swords",
                          "xbows_guns"};
    for (const char *set_name : sets) {
        auto had_data = vfs->read(std::string("wear/") + set_name + "/" + set_name + ".had");
        REQUIRE(had_data.has_value());
        auto had = gl::assets::parse_wear_had(had_data.value());
        REQUIRE(had.has_value());
        CHECK(!had->empty());
        for (const auto &entry : vfs->list()) {
            if (entry.path.rfind(std::string("wear/") + set_name + "/", 0) != 0)
                continue;
            if (entry.path.size() < 4)
                continue;
            const std::string ext = entry.path.substr(entry.path.size() - 4);
            if (ext == ".iad") {
                auto data = vfs->read(entry.path);
                REQUIRE(data.has_value());
                CHECK(gl::assets::parse_iad(data.value()).has_value());
            } else if (ext == ".seq") {
                auto data = vfs->read(entry.path);
                REQUIRE(data.has_value());
                CHECK(gl::assets::parse_wear_seq(data.value()).has_value());
            }
        }
    }
}
