#include "gl/assets/csx.hpp"
#include "gl/assets/pad.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/person.hpp"

#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;

void put_u16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void put_i16(std::vector<std::uint8_t> &out, std::int16_t value) {
    put_u16(out, std::bit_cast<std::uint16_t>(value));
}

void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

void put_i32(std::vector<std::uint8_t> &out, std::int32_t value) {
    put_u32(out, std::bit_cast<std::uint32_t>(value));
}

void put_f32(std::vector<std::uint8_t> &out, float value) {
    put_u32(out, std::bit_cast<std::uint32_t>(value));
}

void set_u32(std::vector<std::uint8_t> &out, std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out[offset + shift / 8] = static_cast<std::uint8_t>((value >> shift) & 0xFF);
}

void set_i32(std::vector<std::uint8_t> &out, std::size_t offset, std::int32_t value) {
    set_u32(out, offset, std::bit_cast<std::uint32_t>(value));
}

void put_animation(std::vector<std::uint8_t> &out, gl::assets::PadAnimationType type,
                   std::int32_t delay, std::int32_t frames_per_row, std::int32_t frame_width,
                   std::int32_t frame_height, std::int32_t rows, std::int32_t shadow_width,
                   std::int32_t shadow_height, std::int32_t shadow_rows, float move_x,
                   float move_y, std::int32_t anchor_x, std::int32_t anchor_y,
                   std::int32_t shadow_anchor_x, std::int32_t shadow_anchor_y) {
    put_u32(out, static_cast<std::uint32_t>(type));
    const std::size_t size_offset = out.size();
    put_i32(out, 0);
    put_i32(out, delay);
    put_i32(out, frames_per_row);
    put_i32(out, frame_width);
    put_i32(out, frame_height);
    put_i32(out, anchor_x);
    put_i32(out, anchor_y);
    put_f32(out, move_x);
    put_f32(out, move_y);
    put_u32(out, static_cast<std::uint32_t>(rows * frames_per_row * 8));
    for (std::int32_t row = 0; row < rows; ++row) {
        for (std::int32_t frame = 0; frame < frames_per_row; ++frame) {
            put_i16(out, static_cast<std::int16_t>(row));
            put_i16(out, static_cast<std::int16_t>(frame));
            put_i16(out, static_cast<std::int16_t>(frame_width));
            put_i16(out, static_cast<std::int16_t>(frame_height));
        }
    }
    put_i32(out, shadow_width);
    put_i32(out, shadow_height);
    put_i32(out, shadow_anchor_x);
    put_i32(out, shadow_anchor_y);
    put_u32(out, static_cast<std::uint32_t>(shadow_rows * frames_per_row * 8));
    for (std::int32_t row = 0; row < shadow_rows; ++row) {
        for (std::int32_t frame = 0; frame < frames_per_row; ++frame) {
            put_i16(out, static_cast<std::int16_t>(row));
            put_i16(out, static_cast<std::int16_t>(frame));
            put_i16(out, static_cast<std::int16_t>(shadow_width));
            put_i16(out, static_cast<std::int16_t>(shadow_height));
        }
    }
    set_i32(out, size_offset, static_cast<std::int32_t>(out.size() - size_offset));
}

std::vector<std::uint8_t> make_pad(bool with_tb_go = false) {
    constexpr std::int32_t kDelay = 50;
    constexpr std::int32_t kFramesPerRow = 2;
    constexpr std::int32_t kFrameWidth = 10;
    constexpr std::int32_t kFrameHeight = 20;
    constexpr std::int32_t kRows = 5;
    constexpr std::int32_t kShadowWidth = 6;
    constexpr std::int32_t kShadowHeight = 7;
    constexpr std::int32_t kShadowRows = 8;

    std::uint32_t masks = static_cast<std::uint32_t>(gl::assets::PadAnimationType::rt_stay);
    if (with_tb_go)
        masks |= static_cast<std::uint32_t>(gl::assets::PadAnimationType::tb_go);

    std::vector<std::uint8_t> out;
    out.insert(out.end(), {'P', 'A', 'D', ' '});
    put_u32(out, 0);
    put_u32(out, masks);
    put_animation(out, gl::assets::PadAnimationType::rt_stay, kDelay, kFramesPerRow, kFrameWidth,
                  kFrameHeight, kRows, kShadowWidth, kShadowHeight, kShadowRows, 1.5F, -2.0F, 5,
                  10, 1, 2);
    if (with_tb_go)
        put_animation(out, gl::assets::PadAnimationType::tb_go, 40, kFramesPerRow, kFrameWidth,
                      kFrameHeight, kRows, kShadowWidth, kShadowHeight, kShadowRows, 4.0F, 3.0F, 5,
                      10, 1, 2);
    set_u32(out, 4, static_cast<std::uint32_t>(out.size()));
    return out;
}

std::vector<std::uint8_t> make_transparent_csx(std::uint32_t width, std::uint32_t height) {
    const std::vector<std::uint32_t> palette = {0x000000, 0x00FF00};
    std::vector<std::uint8_t> rows;
    std::vector<std::uint32_t> offsets;
    offsets.push_back(0);
    for (std::uint32_t y = 0; y < height; ++y) {
        rows.push_back(0x6C);
        rows.push_back(static_cast<std::uint8_t>(width));
        offsets.push_back(static_cast<std::uint32_t>(rows.size()));
    }

    std::vector<std::uint8_t> out;
    put_u32(out, static_cast<std::uint32_t>(palette.size()));
    put_u32(out, palette[1]);
    for (std::uint32_t color : palette)
        put_u32(out, color);
    put_u32(out, width);
    put_u32(out, height);
    for (std::uint32_t offset : offsets)
        put_u32(out, offset);
    out.insert(out.end(), rows.begin(), rows.end());
    return out;
}

fs::path unique_temp_dir() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() / ("gl_person_loader_" + std::to_string(stamp) + "_" +
                                        std::to_string(counter.fetch_add(1)));
}

struct TempDir {
    TempDir() : path(unique_temp_dir()) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code error;
        fs::remove_all(path, error);
    }
    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;

    fs::path path;
};

void write_bytes(const fs::path &path, const std::vector<std::uint8_t> &bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out.good());
}

void write_text(const fs::path &path, std::string_view text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    REQUIRE(out.good());
}

void write_person_assets(const fs::path &root, std::string_view res_name,
                         bool with_tb_go = false) {
    std::string lower(res_name);
    for (char &c : lower) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (c == '\\')
            c = '/';
    }
    const fs::path directory = root / "persons" / lower;
    write_bytes(directory / (lower + ".pad"), make_pad(with_tb_go));
    write_bytes(directory / "animation" / "rt_stay.csx", make_transparent_csx(20, 100));
}

void write_shadow(const fs::path &root, std::string_view res_name) {
    std::string lower(res_name);
    for (char &c : lower) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (c == '\\')
            c = '/';
    }
    write_bytes(root / "persons" / lower / "shadows" / "rt_stay.csx", make_transparent_csx(12, 56));
}

void write_script(const fs::path &root, std::string_view name, std::string_view res_name) {
    std::string lower(name);
    for (char &c : lower) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    write_text(root / "scripts" / "persons" / (lower + ".scr"),
               "res_name: \"" + std::string(res_name) + "\"\nlevel: 1\n");
}

void write_sef(const fs::path &root, std::string_view level, std::string_view persons) {
    write_text(root / "levels" / "single" / std::string(level) / (std::string(level) + ".sef"),
               "pack: \"" + std::string(level) + "\"\npersons:\n{\n" + std::string(persons) +
                   "}\n");
}

void write_sef_with_cells(const fs::path &root, std::string_view level, std::string_view persons,
                          std::string_view cell_groups) {
    write_text(root / "levels" / "single" / std::string(level) / (std::string(level) + ".sef"),
               "pack: \"" + std::string(level) + "\"\npersons:\n{\n" + std::string(persons) +
                   "\n}\ncell_groups:\n{\n" + std::string(cell_groups) + "\n}\n");
}
} // namespace

TEST_CASE("person loader: loads PAD, sprite, shadow and SEF placement") {
    TempDir dir;
    write_sef(dir.path, "test_level",
              "name: \"Synthetic.Hero\"\n"
              "{\n"
              "position 3 4\n"
              "direction \"UP_LEFT\"\n"
              "route_type \"PATCH\"\n"
              "route \"track1\"\n"
              "radius 7\n"
              "delay_min 1\n"
              "delay_max 2\n"
              "tribe \"testers\"\n"
              "scr_dialog \"d\\x.d1.age\"\n"
              "scr_inv \"inv001\"\n"
              "}\n");
    write_script(dir.path, "Synthetic.Hero", "L0.Synthetic_Sheet");
    write_person_assets(dir.path, "L0.Synthetic_Sheet");
    write_shadow(dir.path, "L0.Synthetic_Sheet");

    auto vfs = gl::assets::GameVfs::mount(dir.path);
    REQUIRE(vfs.has_value());
    auto persons = gl::game::load_persons(vfs.value(), "test_level");
    REQUIRE(persons.has_value());
    REQUIRE(persons->size() == 1);

    const gl::game::PersonInstance &person = persons->front();
    CHECK(person.name == "Synthetic.Hero");
    CHECK(person.tile_x == 3);
    CHECK(person.tile_y == 4);
    CHECK(person.direction == 1);
    CHECK(person.route_type == "PATCH");
    CHECK(person.route == "track1");
    CHECK(person.radius == 7);
    CHECK(person.delay_min == 1);
    CHECK(person.delay_max == 2);
    CHECK(person.tribe == "testers");
    CHECK(person.dialog == "d\\x.d1.age");
    CHECK(person.inventory == "inv001");
    CHECK(person.res_name == "L0.Synthetic_Sheet");
    CHECK(person.def.res_name == "L0.Synthetic_Sheet");
    CHECK(person.def.level == 1);
    REQUIRE(person.pad.size() == 1);
    CHECK(person.pad[0].type == gl::assets::PadAnimationType::rt_stay);
    CHECK(person.stay_type == gl::assets::PadAnimationType::rt_stay);
    CHECK(person.pad[0].delay == 50);
    CHECK(person.pad[0].frames_per_row == 2);
    CHECK(person.pad[0].frame_width == 10);
    CHECK(person.pad[0].frame_height == 20);
    CHECK(person.pad[0].crops.size() == 5);
    CHECK(person.sprite.width == 20);
    CHECK(person.sprite.height == 100);
    CHECK(person.shadow.width == 12);
    CHECK(person.shadow.height == 56);
}

TEST_CASE("person loader: missing NPC resources are skipped with a warning") {
    TempDir dir;
    write_sef(dir.path, "test_level",
              "name: \"Good.One\"\n"
              "{\n"
              "position 1 2\n"
              "direction \"DOWN\"\n"
              "}\n"
              "name: \"Missing.Script\"\n"
              "{\n"
              "position 3 4\n"
              "direction \"UP\"\n"
              "}\n"
              "name: \"Missing.Pad\"\n"
              "{\n"
              "position 5 6\n"
              "direction \"LEFT\"\n"
              "}\n"
              "name: \"Missing.Sprite\"\n"
              "{\n"
              "position 7 8\n"
              "direction \"RIGHT\"\n"
              "}\n"
              "name: \"No.Shadow\"\n"
              "{\n"
              "position 9 10\n"
              "direction \"DOWN_RIGHT\"\n"
              "}\n");
    write_script(dir.path, "Good.One", "L0.Good_One");
    write_person_assets(dir.path, "L0.Good_One");
    write_shadow(dir.path, "L0.Good_One");

    write_script(dir.path, "Missing.Pad", "L0.No_Pad");
    write_bytes(dir.path / "persons" / "l0.no_pad" / "animation" / "rt_stay.csx",
                make_transparent_csx(20, 100));

    write_script(dir.path, "Missing.Sprite", "L0.No_Sprite");
    write_bytes(dir.path / "persons" / "l0.no_sprite" / "l0.no_sprite.pad", make_pad());

    write_script(dir.path, "No.Shadow", "L0.No_Shadow");
    write_person_assets(dir.path, "L0.No_Shadow");

    auto vfs = gl::assets::GameVfs::mount(dir.path);
    REQUIRE(vfs.has_value());
    auto persons = gl::game::load_persons(vfs.value(), "test_level");
    REQUIRE(persons.has_value());
    REQUIRE(persons->size() == 2);
    CHECK(persons->at(0).name == "Good.One");
    CHECK(persons->at(1).name == "No.Shadow");
    CHECK(persons->at(1).shadow.width == 0);
    CHECK(persons->at(1).shadow.height == 0);
}

TEST_CASE("person loader: resolves route cells from cell_groups") {
    TempDir dir;
    write_sef_with_cells(dir.path, "test_level",
                         "name: \"Walk.One\"\n{\n  position 10 10\n  direction \"DOWN\"\n"
                         "  route_type \"MOVED\"\n  route \"track1\"\n  radius 15\n"
                         "  delay_min 0\n  delay_max 9000\n}\n",
                         "name: \"track1\"\n{\n  cell_00 10 10\n  cell_01 13 10\n}\n");
    write_script(dir.path, "Walk.One", "L0.Walk_One");
    write_person_assets(dir.path, "L0.Walk_One");
    write_shadow(dir.path, "L0.Walk_One");

    auto vfs = gl::assets::GameVfs::mount(dir.path.string());
    REQUIRE(vfs.has_value());
    auto persons = gl::game::load_persons(vfs.value(), "test_level");
    REQUIRE(persons.has_value());
    REQUIRE(persons->size() == 1);
    REQUIRE(persons->front().route_cells.size() == 2);
    CHECK(persons->front().route_cells[0] == std::pair<std::int32_t, std::int32_t>{10, 10});
    CHECK(persons->front().route_cells[1] == std::pair<std::int32_t, std::int32_t>{13, 10});
    CHECK_FALSE(persons->front().go_type.has_value());
}

TEST_CASE("person loader: tb_go animation is used for movement") {
    TempDir dir;
    write_sef_with_cells(dir.path, "test_level",
                         "name: \"Walk.Tb\"\n{\n  position 10 10\n  direction \"DOWN\"\n"
                         "  route_type \"MOVED\"\n  route \"track1\"\n  radius 15\n"
                         "  delay_min 0\n  delay_max 9000\n}\n",
                         "name: \"track1\"\n{\n  cell_00 10 10\n  cell_01 13 10\n}\n");
    write_script(dir.path, "Walk.Tb", "L0.Tb_Sheet");
    write_person_assets(dir.path, "L0.Tb_Sheet", true);

    auto vfs = gl::assets::GameVfs::mount(dir.path);
    REQUIRE(vfs.has_value());
    auto persons = gl::game::load_persons(vfs.value(), "test_level");
    REQUIRE(persons.has_value());
    REQUIRE(persons->size() == 1);
    const gl::game::PersonInstance &person = persons->front();
    REQUIRE(person.go_type.has_value());
    CHECK(person.go_type == gl::assets::PadAnimationType::tb_go);
    CHECK(person.go_delay_ms == 40);
    CHECK(person.go_move_x == doctest::Approx(4.0F));
    CHECK(person.go_move_y == doctest::Approx(3.0F));
}
