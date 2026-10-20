#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <stb_image.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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

#ifndef GLGAME_EXE
#error "GLGAME_EXE must be defined by the build"
#endif

namespace {
namespace fs = std::filesystem;

const fs::path g_glgame_exe = GLGAME_EXE;

fs::path unique_temp_dir() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
           ("gl_glgame_shot_" + std::to_string(stamp) + "_" + std::to_string(counter.fetch_add(1)));
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

int run_glgame(const std::vector<std::string>& arguments) {
    use_dummy_video();

#ifdef _WIN32
    std::vector<std::string> owned;
    owned.push_back(quote_argument(g_glgame_exe.string()));
    for (const auto& arg : arguments) owned.push_back(quote_argument(arg));
    std::vector<const char*> argv;
    argv.reserve(owned.size() + 1);
    for (const auto& arg : owned) argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    return static_cast<int>(_spawnv(_P_WAIT, g_glgame_exe.string().c_str(), argv.data()));
#else
    std::string command = "\"" + g_glgame_exe.string() + "\"";
    for (const auto& arg : arguments) command += " \"" + arg + "\"";
    const int status = std::system(command.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif
}

std::vector<std::string> screenshot_arguments(const fs::path& game_dir, const fs::path& png,
                                              const std::vector<std::string>& extra = {}) {
    std::vector<std::string> arguments = {
        "--game-dir", game_dir.string(), "--screenshot", png.string(),
        "--width",    "1280",             "--height",    "720"};
    arguments.insert(arguments.end(), extra.begin(), extra.end());
    return arguments;
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::vector<std::string> split_lines(const std::string& bytes) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= bytes.size()) {
        const std::size_t end = bytes.find('\n', start);
        std::string line = end == std::string::npos ? bytes.substr(start)
                                                    : bytes.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return lines;
}

std::string trim_left(std::string line) {
    const std::size_t first = line.find_first_not_of(" \t");
    return first == std::string::npos ? std::string{} : line.substr(first);
}

bool has_line(const std::vector<std::string>& lines, const std::string& expected) {
    for (const std::string& line : lines) {
        if (line == expected) return true;
    }
    return false;
}

void check_png(const fs::path& png, int expected_width, int expected_height) {
    int width = 0;
    int height = 0;
    int channels = 0;
    const StbiImage pixels = load_png(png, width, height, channels);
    REQUIRE(pixels != nullptr);
    CHECK(width == expected_width);
    CHECK(height == expected_height);

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
} // namespace

int main(int argc, char** argv) {
    if (std::getenv("GL_GAME_DIR") == nullptr) return 77;
    doctest::Context context(argc, argv);
    return context.run();
}

TEST_CASE("glgame renders the menu into a 1280x720 PNG") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path png = temp.path / "menu.png";
    const int code = run_glgame(screenshot_arguments(game_dir, png, {"--menu"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);
    check_png(png, 1280, 720);
}

TEST_CASE("glgame renders a preloaded level into a 1280x720 PNG") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path png = temp.path / "level.png";
    const int code =
        run_glgame(screenshot_arguments(game_dir, png, {"--level", "l1_1", "--exec", "gv_pause 1"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);
    check_png(png, 1280, 720);
}

TEST_CASE("glgame tolerates help and unknown commands from --exec") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path png = temp.path / "exec.png";
    const int code = run_glgame(
        screenshot_arguments(game_dir, png, {"--menu", "--exec", "help", "--exec", "unknown"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);
    check_png(png, 1280, 720);
}

TEST_CASE("glgame persists cl_username and keeps binds/comments") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    const fs::path source_config = fs::path(game_dir) / "GoldenLand.ini";
    REQUIRE(fs::exists(source_config));

    TempDir temp;
    const fs::path config = temp.path / "GoldenLand.ini";
    fs::copy_file(source_config, config);
    const std::string before = read_file(config);
    const std::vector<std::string> before_lines = split_lines(before);

    const fs::path png = temp.path / "config.png";
    const int code = run_glgame(screenshot_arguments(
        game_dir, png, {"--config", config.string(), "--menu", "--exec", "cl_username TestUser"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);

    const std::string after = read_file(config);
    const std::vector<std::string> after_lines = split_lines(after);

    bool username_found = false;
    for (const std::string& line : after_lines) {
        const std::string trimmed = trim_left(line);
        if (trimmed.rfind("cl_username", 0) == 0 && trimmed.find("TestUser") != std::string::npos) {
            username_found = true;
        }
    }
    CHECK(username_found);

    std::size_t preserved = 0;
    for (const std::string& line : before_lines) {
        const std::string trimmed = trim_left(line);
        if (trimmed.rfind("bind", 0) == 0 || trimmed.rfind("//", 0) == 0) {
            REQUIRE(has_line(after_lines, line));
            ++preserved;
        }
    }
    CAPTURE(preserved);
    CHECK(preserved > 0);
}
