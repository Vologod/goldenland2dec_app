#include <doctest/doctest.h>
#include "gl/core/config.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::filesystem::path unique_temp_file() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("gl_real_config_" + std::to_string(stamp) + "_" +
            std::to_string(counter.fetch_add(1)) + ".ini");
}

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
}

TEST_CASE("Config: real GoldenLand.ini round-trips byte-identically" *
          doctest::skip(!game_dir_available())) {
    const char* dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    const std::filesystem::path input = std::filesystem::path(dir) / "GoldenLand.ini";

    auto loaded = gl::core::Config::load(input);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->raw_lines().empty());

    const std::filesystem::path output = unique_temp_file();
    REQUIRE(loaded->save(output).has_value());
    CHECK(read_bytes(output) == read_bytes(input));

    std::filesystem::remove(output);
}
