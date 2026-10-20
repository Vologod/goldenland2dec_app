#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <stb_image.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <process.h>
#endif

#ifndef GLVIEW_EXE
#error "GLVIEW_EXE must be defined by the build"
#endif

namespace {
namespace fs = std::filesystem;

const fs::path g_glview_exe = GLVIEW_EXE;

fs::path unique_temp_dir() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
           ("gl_glview_shot_" + std::to_string(stamp) + "_" + std::to_string(counter.fetch_add(1)));
}

std::vector<std::string> glview_arguments(const fs::path& game_dir, const fs::path& png) {
    return {"--game-dir", game_dir.string(), "--level", "l1_1", "--screenshot", png.string(),
            "--width", "1280", "--height", "720"};
}

int run_glview(const std::vector<std::string>& arguments) {
#ifdef _WIN32
    _putenv_s("SDL_VIDEODRIVER", "dummy");
    _putenv_s("SDL_RENDER_DRIVER", "software");

    std::vector<std::string> owned;
    owned.push_back(g_glview_exe.string());
    for (const auto& arg : arguments) owned.push_back(arg);
    std::vector<const char*> argv;
    argv.reserve(owned.size() + 1);
    for (const auto& arg : owned) argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    return static_cast<int>(_spawnv(_P_WAIT, g_glview_exe.string().c_str(), argv.data()));
#else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_RENDER_DRIVER", "software", 1);

    std::string command = "\"" + g_glview_exe.string() + "\"";
    for (const auto& arg : arguments) command += " \"" + arg + "\"";
    const int status = std::system(command.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif
}
} // namespace

int main(int argc, char** argv) {
    if (std::getenv("GL_GAME_DIR") == nullptr) return 77;
    doctest::Context context(argc, argv);
    return context.run();
}

TEST_CASE("glview renders the l1_1 background into a 1280x720 PNG") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    const fs::path temp = unique_temp_dir();
    fs::remove_all(temp);
    fs::create_directories(temp);
    const fs::path png = temp / "l1_1.png";

    const int code = run_glview(glview_arguments(game_dir, png));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(png.string().c_str(), &width, &height, &channels, 4);
    REQUIRE(pixels != nullptr);
    CHECK(width == 1280);
    CHECK(height == 720);

    std::set<std::uint32_t> colors;
    for (int i = 0; i < width * height; ++i) {
        const unsigned char* pixel = pixels + static_cast<std::size_t>(i) * 4;
        colors.insert((static_cast<std::uint32_t>(pixel[0]) << 24) |
                      (static_cast<std::uint32_t>(pixel[1]) << 16) |
                      (static_cast<std::uint32_t>(pixel[2]) << 8) |
                      static_cast<std::uint32_t>(pixel[3]));
    }
    stbi_image_free(pixels);

    CAPTURE(colors.size());
    CHECK(colors.size() >= 2);

    fs::remove_all(temp);
}

TEST_CASE("glview exits 3 when required level data is missing") {
    const fs::path temp = unique_temp_dir();
    fs::remove_all(temp);
    const fs::path game_dir = temp / "game";
    fs::create_directories(game_dir);
    const fs::path png = temp / "missing.png";

    const std::vector<std::string> arguments = {
        "--game-dir", game_dir.string(), "--level", "l1_1", "--screenshot", png.string(),
        "--width", "1280", "--height", "720"};
    const int code = run_glview(arguments);
    CAPTURE(code);
    CHECK(code == 3);
    CHECK_FALSE(fs::exists(png));

    fs::remove_all(temp);
}
