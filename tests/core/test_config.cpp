#include <doctest/doctest.h>
#include "gl/core/config.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>

namespace {
std::filesystem::path unique_temp_dir() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("gl_core_test_" + std::to_string(stamp) + "_" + std::to_string(counter.fetch_add(1)));
}

std::string cp1251_bytes(std::initializer_list<std::uint8_t> bytes) {
    std::string out;
    for (std::uint8_t byte : bytes) out.push_back(static_cast<char>(byte));
    return out;
}

void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.is_open());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out.good());
}

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string make_synthetic_ini() {
    std::string bytes;
    bytes += "// header\r\n";
    bytes += "\r\n";
    bytes += "cl_username\t\t\t\t\t";
    bytes += cp1251_bytes({0xC2, 0xE0, 0xF1, 0xFF});
    bytes += "\r\n";
    bytes += "r_windowed 1\r\n";
    bytes += "gv_minimap_alpha 0.8 // transparency\r\n";
    bytes += "d_console 1\r\n";
    bytes += "bad_int abc\r\n";
    bytes += "bad_float 0.8x\r\n";
    bytes += "bad_bool 2\r\n";
    bytes += "bind \tDIK_F8, vk_screenshot\r\n";
    bytes += "unknown_key  spaced value  \r\n";
    bytes += "// mixed endings\n";
    bytes += "last_value\tline\n";
    return bytes;
}
}

TEST_CASE("Config: reads strings, numbers and bools; strips inline comments") {
    const auto dir = unique_temp_dir();
    const auto path = dir / "GoldenLand.ini";
    write_bytes(path, make_synthetic_ini());

    auto loaded = gl::core::Config::load(path);
    REQUIRE(loaded.has_value());
    const gl::core::Config& config = loaded.value();

    const auto username = config.get("cl_username");
    REQUIRE(username.has_value());
    CHECK(*username == "Вася");

    CHECK(config.get_int("r_windowed") == 1);
    CHECK(config.get_bool("r_windowed", false));
    CHECK(config.get_bool("d_console", false));

    const auto alpha = config.get("gv_minimap_alpha");
    REQUIRE(alpha.has_value());
    CHECK(*alpha == "0.8");
    const auto alpha_value = config.get_float("gv_minimap_alpha");
    REQUIRE(alpha_value.has_value());
    CHECK(*alpha_value == doctest::Approx(0.8));

    const auto unknown = config.get("unknown_key");
    REQUIRE(unknown.has_value());
    CHECK(*unknown == "spaced value");

    CHECK_FALSE(config.get("bind").has_value());
    CHECK_FALSE(config.get("R_WINDOWED").has_value());
    CHECK_FALSE(config.get("missing").has_value());
    CHECK_FALSE(config.get_int("cl_username").has_value());
    CHECK_FALSE(config.get_int("bad_int").has_value());
    CHECK_FALSE(config.get_int("missing").has_value());
    CHECK_FALSE(config.get_float("bad_float").has_value());
    CHECK_FALSE(config.get_float("cl_username").has_value());
    CHECK_FALSE(config.get_float("missing").has_value());
    CHECK(config.get_bool("bad_bool", true));
    CHECK_FALSE(config.get_bool("bad_bool", false));
    CHECK(config.get_bool("missing", true));
    CHECK_FALSE(config.get_bool("missing", false));

    REQUIRE(config.raw_lines().size() == 13);
    CHECK(config.raw_lines()[0] == "// header");
    CHECK(config.raw_lines()[1].empty());
    CHECK(config.raw_lines()[2] == "cl_username\t\t\t\t\tВася");
    CHECK(config.raw_lines()[11] == "// mixed endings");
    CHECK(config.raw_lines()[12] == "last_value\tline");

    std::filesystem::remove_all(dir);
}

TEST_CASE("Config: set preserves layout, comments and order; save reload") {
    const auto dir = unique_temp_dir();
    const auto input = dir / "GoldenLand.ini";
    const auto output = dir / "saved" / "GoldenLand.ini";
    write_bytes(input, make_synthetic_ini());

    auto loaded = gl::core::Config::load(input);
    REQUIRE(loaded.has_value());
    gl::core::Config config = loaded.value();

    config.set("r_windowed", "0");
    config.set_int("d_console", 0);
    config.set("cl_username", "Пётр");
    config.set("gv_minimap_alpha", "0.5");
    config.set("new_key", "new value");

    REQUIRE(config.save(output).has_value());

    const std::string bytes = read_bytes(output);
    CHECK(bytes.find("r_windowed 0\r\n") != std::string::npos);
    CHECK(bytes.find("cl_username\t\t\t\t\t" + cp1251_bytes({0xCF, 0xB8, 0xF2, 0xF0}) + "\r\n") !=
          std::string::npos);
    CHECK(bytes.find("gv_minimap_alpha 0.5 // transparency\r\n") != std::string::npos);
    CHECK(bytes.find("bind \tDIK_F8, vk_screenshot\r\n") != std::string::npos);
    CHECK(bytes.find("new_key new value\r\n") != std::string::npos);

    const auto header_pos = bytes.find("// header");
    const auto bind_pos = bytes.find("bind");
    const auto new_pos = bytes.find("new_key");
    REQUIRE(header_pos != std::string::npos);
    REQUIRE(bind_pos != std::string::npos);
    REQUIRE(new_pos != std::string::npos);
    CHECK(header_pos < bind_pos);
    CHECK(bind_pos < new_pos);

    auto reloaded = gl::core::Config::load(output);
    REQUIRE(reloaded.has_value());
    const gl::core::Config& again = reloaded.value();
    const auto username = again.get("cl_username");
    REQUIRE(username.has_value());
    CHECK(*username == "Пётр");
    CHECK(again.get_int("r_windowed") == 0);
    CHECK_FALSE(again.get_bool("d_console", true));
    const auto alpha = again.get_float("gv_minimap_alpha");
    REQUIRE(alpha.has_value());
    CHECK(*alpha == doctest::Approx(0.5));
    CHECK(again.get("new_key") == "new value");

    std::filesystem::remove_all(dir);
}

TEST_CASE("Config: untouched file round-trips byte-identically with mixed EOLs") {
    const auto dir = unique_temp_dir();
    const auto input = dir / "GoldenLand.ini";
    const auto output = dir / "copy.ini";
    const std::string original = make_synthetic_ini();
    write_bytes(input, original);

    auto loaded = gl::core::Config::load(input);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->save(output).has_value());

    const std::string round_tripped = read_bytes(output);
    CHECK(round_tripped == original);
    CHECK(round_tripped.find("\r\n") != std::string::npos);
    CHECK(round_tripped.find("last_value\tline\n") != std::string::npos);

    std::filesystem::remove_all(dir);
}

TEST_CASE("Config: missing file is an error; save creates parent directories") {
    const auto dir = unique_temp_dir();
    const auto missing = dir / "missing.ini";

    auto failed = gl::core::Config::load(missing);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code == gl::ErrorCode::io);

    const auto source = dir / "source.ini";
    write_bytes(source, "value 42\r\n");
    auto loaded = gl::core::Config::load(source);
    REQUIRE(loaded.has_value());

    const auto nested = dir / "nested" / "deeper" / "out.ini";
    REQUIRE(loaded->save(nested).has_value());
    CHECK(std::filesystem::exists(nested));

    auto reloaded = gl::core::Config::load(nested);
    REQUIRE(reloaded.has_value());
    CHECK(reloaded->get_int("value") == 42);

    std::filesystem::remove_all(dir);
}
