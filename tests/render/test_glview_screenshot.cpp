#define DOCTEST_CONFIG_IMPLEMENT
#include "gl/assets/image.hpp"
#include "gl/platform/sdl_context.hpp"
#include "gl/render/texture.hpp"

#include <SDL.h>
#include <doctest.h>
#include <stb_image.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <system_error>
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

struct TempDir {
    TempDir() : path(unique_temp_dir()) {
        fs::remove_all(path);
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code error;
        fs::remove_all(path, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    fs::path path;
};

struct StbiImageDeleter {
    void operator()(unsigned char* pixels) const noexcept { stbi_image_free(pixels); }
};

using StbiImage = std::unique_ptr<unsigned char, StbiImageDeleter>;

StbiImage load_png(const fs::path& path, int& width, int& height, int& channels) {
    return StbiImage(stbi_load(path.string().c_str(), &width, &height, &channels, 4));
}

void use_dummy_video() {
#ifdef _WIN32
    _putenv_s("SDL_VIDEODRIVER", "dummy");
    _putenv_s("SDL_RENDER_DRIVER", "software");
#else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_RENDER_DRIVER", "software", 1);
#endif
}

std::vector<std::string> glview_arguments(const fs::path& game_dir, const fs::path& png,
                                          const std::vector<std::string>& extra = {}) {
    std::vector<std::string> arguments = {"--game-dir", game_dir.string(), "--level", "l1_1",
                                          "--screenshot", png.string(), "--width", "1280",
                                          "--height", "720"};
    arguments.insert(arguments.end(), extra.begin(), extra.end());
    return arguments;
}

int run_glview(const std::vector<std::string>& arguments) {
    use_dummy_video();

#ifdef _WIN32
    std::vector<std::string> owned;
    owned.push_back(g_glview_exe.string());
    for (const auto& arg : arguments) owned.push_back(arg);
    std::vector<const char*> argv;
    argv.reserve(owned.size() + 1);
    for (const auto& arg : owned) argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    return static_cast<int>(_spawnv(_P_WAIT, g_glview_exe.string().c_str(), argv.data()));
#else
    std::string command = "\"" + g_glview_exe.string() + "\"";
    for (const auto& arg : arguments) command += " \"" + arg + "\"";
    const int status = std::system(command.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif
}

#ifdef _WIN32
std::string quote_argument(const std::string& arg) {
    if (arg.find_first_of(" \t\"") == std::string::npos) return arg;
    std::string quoted = "\"";
    for (const char c : arg) {
        if (c == '"') quoted += "\\\"";
        else quoted += c;
    }
    quoted += '"';
    return quoted;
}
#endif

int run_glview_capture(const std::vector<std::string>& arguments, std::string& output) {
    use_dummy_video();

#ifdef _WIN32
    std::string command = quote_argument(g_glview_exe.string());
    for (const auto& arg : arguments) command += " " + quote_argument(arg);
    command += " 2>&1";
    FILE* pipe = _popen(command.c_str(), "rb");
#else
    std::string command = "\"" + g_glview_exe.string() + "\"";
    for (const auto& arg : arguments) command += " \"" + arg + "\"";
    command += " 2>&1";
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) return -1;

    char buffer[4096];
    std::size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        output.append(buffer, count);
    }
#ifdef _WIN32
    return _pclose(pipe);
#else
    const int status = pclose(pipe);
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

    TempDir temp;
    const fs::path png = temp.path / "l1_1.png";

    const int code = run_glview(glview_arguments(game_dir, png));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);

    int width = 0;
    int height = 0;
    int channels = 0;
    const StbiImage pixels = load_png(png, width, height, channels);
    REQUIRE(pixels != nullptr);
    CHECK(width == 1280);
    CHECK(height == 720);

    std::set<std::uint32_t> colors;
    for (int i = 0; i < width * height; ++i) {
        const unsigned char* pixel = pixels.get() + static_cast<std::size_t>(i) * 4;
        colors.insert((static_cast<std::uint32_t>(pixel[0]) << 24) |
                      (static_cast<std::uint32_t>(pixel[1]) << 16) |
                      (static_cast<std::uint32_t>(pixel[2]) << 8) |
                      static_cast<std::uint32_t>(pixel[3]));
    }

    CAPTURE(colors.size());
    CHECK(colors.size() >= 2);
}

TEST_CASE("glview draws statics and animations over the background") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path normal_png = temp.path / "normal.png";
    const fs::path background_png = temp.path / "background.png";

    const int normal_code =
        run_glview(glview_arguments(game_dir, normal_png, {"--frames", "5"}));
    const int background_code = run_glview(
        glview_arguments(game_dir, background_png, {"--frames", "5", "--background-only"}));
    CAPTURE(normal_code);
    CAPTURE(background_code);
    REQUIRE(fs::exists(normal_png));
    REQUIRE(fs::exists(background_png));
    CHECK(normal_code == 0);
    CHECK(background_code == 0);

    int normal_width = 0;
    int normal_height = 0;
    int normal_channels = 0;
    const StbiImage normal_pixels =
        load_png(normal_png, normal_width, normal_height, normal_channels);
    REQUIRE(normal_pixels != nullptr);

    int background_width = 0;
    int background_height = 0;
    int background_channels = 0;
    const StbiImage background_pixels =
        load_png(background_png, background_width, background_height, background_channels);
    REQUIRE(background_pixels != nullptr);

    CHECK(normal_width == 1280);
    CHECK(normal_height == 720);
    CHECK(background_width == 1280);
    CHECK(background_height == 720);

    std::uint64_t differing_pixels = 0;
    if (normal_width == background_width && normal_height == background_height) {
        const std::size_t pixel_count =
            static_cast<std::size_t>(normal_width) * static_cast<std::size_t>(normal_height);
        for (std::size_t i = 0; i < pixel_count; ++i) {
            const unsigned char* a = normal_pixels.get() + i * 4;
            const unsigned char* b = background_pixels.get() + i * 4;
            if (a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3]) {
                ++differing_pixels;
            }
        }
    }

    CAPTURE(differing_pixels);
    CHECK(differing_pixels > 1000);
}

TEST_CASE("glview draws NPCs on l1_1 and --no-persons removes them") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path normal_png = temp.path / "persons.png";
    const fs::path no_persons_png = temp.path / "no_persons.png";

    std::string log;
    const int normal_code =
        run_glview_capture(glview_arguments(game_dir, normal_png), log);
    const int no_persons_code = run_glview(
        glview_arguments(game_dir, no_persons_png, {"--no-persons"}));
    CAPTURE(normal_code);
    CAPTURE(no_persons_code);
    CAPTURE(log);
    REQUIRE(fs::exists(normal_png));
    REQUIRE(fs::exists(no_persons_png));
    CHECK(normal_code == 0);
    CHECK(no_persons_code == 0);
    CHECK(log.find("16 persons") != std::string::npos);

    int normal_width = 0;
    int normal_height = 0;
    int normal_channels = 0;
    const StbiImage normal_pixels =
        load_png(normal_png, normal_width, normal_height, normal_channels);
    REQUIRE(normal_pixels != nullptr);

    int plain_width = 0;
    int plain_height = 0;
    int plain_channels = 0;
    const StbiImage plain_pixels =
        load_png(no_persons_png, plain_width, plain_height, plain_channels);
    REQUIRE(plain_pixels != nullptr);

    CHECK(normal_width == 1280);
    CHECK(normal_height == 720);
    CHECK(plain_width == 1280);
    CHECK(plain_height == 720);

    std::uint64_t differing_pixels = 0;
    if (normal_width == plain_width && normal_height == plain_height) {
        const std::size_t pixel_count =
            static_cast<std::size_t>(normal_width) * static_cast<std::size_t>(normal_height);
        for (std::size_t i = 0; i < pixel_count; ++i) {
            const unsigned char* a = normal_pixels.get() + i * 4;
            const unsigned char* b = plain_pixels.get() + i * 4;
            if (a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3]) {
                ++differing_pixels;
            }
        }
    }

    CAPTURE(differing_pixels);
    CHECK(differing_pixels > 1000);
}

TEST_CASE("glview NPCs move between t=0 and t=10s") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    TempDir dir;
    const fs::path first_png = dir.path / "move_first.png";
    const fs::path later_png = dir.path / "move_later.png";
    const fs::path first_plain_png = dir.path / "move_first_plain.png";
    const fs::path later_plain_png = dir.path / "move_later_plain.png";

    std::string log;
    const int first_code =
        run_glview_capture(glview_arguments(game_dir, first_png, {"--sim-ms", "0"}), log);
    const int later_code =
        run_glview(glview_arguments(game_dir, later_png, {"--sim-ms", "10000"}));
    const int first_plain_code = run_glview(glview_arguments(
        game_dir, first_plain_png, {"--sim-ms", "0", "--no-persons"}));
    const int later_plain_code = run_glview(glview_arguments(
        game_dir, later_plain_png, {"--sim-ms", "10000", "--no-persons"}));
    CAPTURE(first_code);
    CAPTURE(later_code);
    CAPTURE(first_plain_code);
    CAPTURE(later_plain_code);
    CAPTURE(log);
    REQUIRE(fs::exists(first_png));
    REQUIRE(fs::exists(later_png));
    REQUIRE(fs::exists(first_plain_png));
    REQUIRE(fs::exists(later_plain_png));
    CHECK(first_code == 0);
    CHECK(later_code == 0);
    CHECK(first_plain_code == 0);
    CHECK(later_plain_code == 0);
    CHECK(log.find("16 persons") != std::string::npos);
    CHECK(log.find("go sheet") == std::string::npos);
    CHECK(log.find("go animation") == std::string::npos);
    CHECK(log.find("go texture") == std::string::npos);

    int aw = 0, ah = 0, ac = 0, bw = 0, bh = 0, bc = 0;
    int cw = 0, ch = 0, cc = 0, dw = 0, dh = 0, dc = 0;
    const StbiImage first = load_png(first_png, aw, ah, ac);
    const StbiImage later = load_png(later_png, bw, bh, bc);
    const StbiImage first_plain = load_png(first_plain_png, cw, ch, cc);
    const StbiImage later_plain = load_png(later_plain_png, dw, dh, dc);
    REQUIRE(first != nullptr);
    REQUIRE(later != nullptr);
    REQUIRE(first_plain != nullptr);
    REQUIRE(later_plain != nullptr);
    CHECK(aw == 1280);
    CHECK(ah == 720);
    CHECK(bw == 1280);
    CHECK(bh == 720);
    CHECK(cw == 1280);
    CHECK(ch == 720);
    CHECK(dw == 1280);
    CHECK(dh == 720);

    std::uint64_t differing = 0;
    if (aw == bw && ah == bh && aw == cw && ah == ch && aw == dw && ah == dh) {
        for (int y = 0; y < ah; ++y) {
            for (int x = 0; x < aw; ++x) {
                const std::size_t i = (static_cast<std::size_t>(y) * aw + x) * 4;
                const bool first_npc = first.get()[i] != first_plain.get()[i] ||
                                       first.get()[i + 1] != first_plain.get()[i + 1] ||
                                       first.get()[i + 2] != first_plain.get()[i + 2];
                const bool later_npc = later.get()[i] != later_plain.get()[i] ||
                                       later.get()[i + 1] != later_plain.get()[i + 1] ||
                                       later.get()[i + 2] != later_plain.get()[i + 2];
                if (first_npc != later_npc) ++differing;
            }
        }
    }
    CAPTURE(differing);
    CHECK(differing > 1000);
}

TEST_CASE("glview animation frame changes after crossing the delay boundary") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path first_png = temp.path / "first.png";
    const fs::path later_png = temp.path / "later.png";

    const int first_code = run_glview(glview_arguments(game_dir, first_png, {"--frames", "1"}));
    const int later_code = run_glview(glview_arguments(game_dir, later_png, {"--frames", "8"}));
    CAPTURE(first_code);
    CAPTURE(later_code);
    REQUIRE(fs::exists(first_png));
    REQUIRE(fs::exists(later_png));
    CHECK(first_code == 0);
    CHECK(later_code == 0);

    int first_width = 0;
    int first_height = 0;
    int first_channels = 0;
    const StbiImage first_pixels =
        load_png(first_png, first_width, first_height, first_channels);
    REQUIRE(first_pixels != nullptr);

    int later_width = 0;
    int later_height = 0;
    int later_channels = 0;
    const StbiImage later_pixels =
        load_png(later_png, later_width, later_height, later_channels);
    REQUIRE(later_pixels != nullptr);

    CHECK(first_width == 1280);
    CHECK(first_height == 720);
    CHECK(later_width == 1280);
    CHECK(later_height == 720);

    std::uint64_t differing_pixels = 0;
    if (first_width == later_width && first_height == later_height) {
        const std::size_t pixel_count =
            static_cast<std::size_t>(first_width) * static_cast<std::size_t>(first_height);
        for (std::size_t i = 0; i < pixel_count; ++i) {
            const unsigned char* a = first_pixels.get() + i * 4;
            const unsigned char* b = later_pixels.get() + i * 4;
            if (a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3]) {
                ++differing_pixels;
            }
        }
    }

    CAPTURE(differing_pixels);
    CHECK(differing_pixels > 0);
}

TEST_CASE("glview debug overlay draws visible sprite rectangles") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path plain_png = temp.path / "plain.png";
    const fs::path debug_png = temp.path / "debug.png";

    const int plain_code =
        run_glview(glview_arguments(game_dir, plain_png, {"--frames", "5"}));
    const int debug_code = run_glview(
        glview_arguments(game_dir, debug_png, {"--frames", "5", "--debug-overlay"}));
    CAPTURE(plain_code);
    CAPTURE(debug_code);
    REQUIRE(fs::exists(plain_png));
    REQUIRE(fs::exists(debug_png));
    CHECK(plain_code == 0);
    CHECK(debug_code == 0);

    int plain_width = 0;
    int plain_height = 0;
    int plain_channels = 0;
    const StbiImage plain_pixels =
        load_png(plain_png, plain_width, plain_height, plain_channels);
    REQUIRE(plain_pixels != nullptr);

    int debug_width = 0;
    int debug_height = 0;
    int debug_channels = 0;
    const StbiImage debug_pixels =
        load_png(debug_png, debug_width, debug_height, debug_channels);
    REQUIRE(debug_pixels != nullptr);

    CHECK(plain_width == 1280);
    CHECK(plain_height == 720);
    CHECK(debug_width == 1280);
    CHECK(debug_height == 720);

    std::uint64_t differing_pixels = 0;
    if (plain_width == debug_width && plain_height == debug_height) {
        const std::size_t pixel_count =
            static_cast<std::size_t>(plain_width) * static_cast<std::size_t>(plain_height);
        for (std::size_t i = 0; i < pixel_count; ++i) {
            const unsigned char* a = plain_pixels.get() + i * 4;
            const unsigned char* b = debug_pixels.get() + i * 4;
            if (a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3]) {
                ++differing_pixels;
            }
        }
    }

    CAPTURE(differing_pixels);
    CHECK(differing_pixels > 0);
}

TEST_CASE("glview --walkable-overlay changes the frame") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    TempDir dir;
    const fs::path plain = dir.path / "walk_plain.png";
    const fs::path overlay = dir.path / "walk_overlay.png";
    REQUIRE(run_glview(glview_arguments(game_dir, plain, {"--frames", "5"})) == 0);
    REQUIRE(run_glview(glview_arguments(game_dir, overlay,
                                        {"--frames", "5", "--walkable-overlay"})) == 0);
    int aw = 0, ah = 0, ac = 0, bw = 0, bh = 0, bc = 0;
    const StbiImage a = load_png(plain, aw, ah, ac);
    const StbiImage b = load_png(overlay, bw, bh, bc);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(aw == 1280);
    REQUIRE(ah == 720);
    std::uint64_t differing = 0;
    for (int y = 0; y < ah; ++y)
        for (int x = 0; x < aw; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * aw + x) * 4;
            if (a.get()[i] != b.get()[i] || a.get()[i + 1] != b.get()[i + 1] ||
                a.get()[i + 2] != b.get()[i + 2])
                ++differing;
        }
    CAPTURE(differing);
    CHECK(differing > 10000);
}

TEST_CASE("glview exits 3 when required level data is missing") {
    TempDir temp;
    const fs::path game_dir = temp.path / "game";
    fs::create_directories(game_dir);
    const fs::path png = temp.path / "missing.png";

    const std::vector<std::string> arguments = {
        "--game-dir", game_dir.string(), "--level", "l1_1", "--screenshot", png.string(),
        "--width", "1280", "--height", "720"};
    const int code = run_glview(arguments);
    CAPTURE(code);
    CHECK(code == 3);
    CHECK_FALSE(fs::exists(png));
}
