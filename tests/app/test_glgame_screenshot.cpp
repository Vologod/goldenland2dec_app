#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/assets/sef.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/game/level.hpp>
#include <gl/render/font.hpp>
#include <stb_image.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
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

std::optional<fs::path> system_font_path() { return gl::render::find_system_font(); }

bool can_load_font(const fs::path& path) {
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, 16, 16, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) return false;
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    bool loaded = false;
    if (renderer != nullptr) {
        {
            const gl::Result<gl::render::Font> font =
                gl::render::Font::load_from_file(renderer, path, 20.0f);
            loaded = font.has_value();
        }
        SDL_DestroyRenderer(renderer);
    }
    SDL_FreeSurface(surface);
    return loaded;
}

bool font_available() {
    static const bool available = [] {
        const std::optional<fs::path> path = system_font_path();
        return path.has_value() && can_load_font(*path);
    }();
    return available;
}

std::optional<fs::path> alternate_system_font() {
    static const std::optional<fs::path> alternate = []() -> std::optional<fs::path> {
        const std::optional<fs::path> primary = system_font_path();
        if (!primary) return std::nullopt;
        std::vector<fs::path> directories;
        directories.emplace_back("C:\\Windows\\Fonts");
        if (const char* windir = std::getenv("WINDIR"); windir != nullptr) {
            directories.emplace_back(fs::path(windir) / "Fonts");
        }
        static const char* const names[] = {"consola.ttf", "arial.ttf", "segoeui.ttf",
                                            "tahoma.ttf", "verdana.ttf"};
        for (const fs::path& directory : directories) {
            for (const char* name : names) {
                const fs::path candidate = directory / name;
                if (candidate.filename() == primary->filename()) continue;
                if (can_load_font(candidate)) return candidate;
            }
        }
        return std::nullopt;
    }();
    return alternate;
}

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

int run_glgame_capture(const std::vector<std::string>& arguments, std::string& output) {
    use_dummy_video();

#ifdef _WIN32
    std::string command = quote_argument(g_glgame_exe.string());
    for (const auto& arg : arguments) command += " " + quote_argument(arg);
    command += " 2>&1";
    FILE* pipe = _popen(command.c_str(), "rb");
#else
    std::string command = "\"" + g_glgame_exe.string() + "\"";
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

fs::path write_test_config(const fs::path& dir, bool with_resolution = false,
                           bool console = false) {
    std::error_code error;
    fs::create_directories(dir, error);
    REQUIRE_FALSE(error);
    const fs::path config = dir / "test.ini";
    std::ofstream out(config, std::ios::binary);
    out << "// deterministic test config\n"
           "cl_username Tester\n"
           "d_console "
        << (console ? 1 : 0) << "\n"
           "gv_pause 0\n"
           "r_windowed 1\n"
           "bind DIK_F8, vk_screenshot\n";
    if (with_resolution) out << "r_resolution 640 480\n";
    REQUIRE(out);
    return config;
}

std::vector<std::string> screenshot_arguments(const fs::path& game_dir, const fs::path& config,
                                              const fs::path& png,
                                              const std::vector<std::string>& extra = {}) {
    std::vector<std::string> arguments = {
        "--game-dir", game_dir.string(), "--config",   config.string(),
        "--screenshot", png.string(),    "--width",    "1280",
        "--height",   "720",             "--seed",     "1"};
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

std::string setting_value(const std::vector<std::string>& lines, std::string_view key) {
    for (const std::string& line : lines) {
        const std::string trimmed = trim_left(line);
        if (trimmed.compare(0, key.size(), key) != 0) continue;
        if (trimmed.size() == key.size()) return {};
        if (trimmed[key.size()] != ' ' && trimmed[key.size()] != '\t') continue;
        std::string value = trim_left(trimmed.substr(key.size()));
        const std::size_t end = value.find_first_of(" \t");
        if (end != std::string::npos) value.resize(end);
        return value;
    }
    return {};
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

std::vector<unsigned char> read_png(const fs::path& png, int& width, int& height) {
    int channels = 0;
    const StbiImage pixels = load_png(png, width, height, channels);
    REQUIRE(pixels != nullptr);
    return std::vector<unsigned char>(pixels.get(),
                                      pixels.get() + static_cast<std::size_t>(width) *
                                                         static_cast<std::size_t>(height) * 4);
}

std::size_t count_differences(const std::vector<unsigned char>& first,
                              const std::vector<unsigned char>& second) {
    REQUIRE(first.size() == second.size());
    std::size_t different = 0;
    for (std::size_t index = 0; index < first.size(); ++index) {
        if (first[index] != second[index]) ++different;
    }
    return different;
}

std::size_t count_region_differences(const std::vector<unsigned char>& first,
                                     const std::vector<unsigned char>& second, int width,
                                     int height, int x0, int y0, int region_width,
                                     int region_height) {
    REQUIRE(first.size() == second.size());
    const int x_end = std::min(x0 + region_width, width);
    const int y_end = std::min(y0 + region_height, height);
    std::size_t different = 0;
    for (int y = std::max(0, y0); y < y_end; ++y) {
        for (int x = std::max(0, x0); x < x_end; ++x) {
            const std::size_t index =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(x)) *
                4;
            if (first[index] != second[index] || first[index + 1] != second[index + 1] ||
                first[index + 2] != second[index + 2] || first[index + 3] != second[index + 3]) {
                ++different;
            }
        }
    }
    return different;
}

struct PixelRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

PixelRect inventory_panel_rect(int screen_width, int screen_height) {
    const float scale = std::min(static_cast<float>(screen_width) / 1024.0f,
                                 static_cast<float>(screen_height) / 768.0f);
    const float offset_x = (static_cast<float>(screen_width) - 1024.0f * scale) * 0.5f;
    const float offset_y = (static_cast<float>(screen_height) - 768.0f * scale) * 0.5f;
    return PixelRect{
        static_cast<int>(std::floor(offset_x + 654.0f * scale)),
        static_cast<int>(std::floor(offset_y + 44.0f * scale)),
        static_cast<int>(std::ceil(360.0f * scale)),
        static_cast<int>(std::ceil(520.0f * scale)),
    };
}

std::size_t count_bottom_differences(const std::vector<unsigned char>& first,
                                     const std::vector<unsigned char>& second, int width,
                                     int height, int bottom_height) {
    REQUIRE(first.size() == second.size());
    const int start = std::max(height - bottom_height, 0);
    std::size_t different = 0;
    for (int y = start; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(x)) *
                4;
            if (first[index] != second[index] || first[index + 1] != second[index + 1] ||
                first[index + 2] != second[index + 2] || first[index + 3] != second[index + 3]) {
                ++different;
            }
        }
    }
    return different;
}

std::optional<gl::game::Level> load_test_level(const fs::path& game_dir) {
    auto vfs = gl::assets::GameVfs::mount(game_dir);
    if (!vfs) return std::nullopt;
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    if (!level) return std::nullopt;
    return std::move(level.value());
}

std::optional<std::size_t> level_person_index(const fs::path& game_dir, std::string_view name) {
    auto level = load_test_level(game_dir);
    if (!level) return std::nullopt;
    for (std::size_t index = 0; index < level->persons.size(); ++index) {
        if (level->persons[index].name == name) return index;
    }
    return std::nullopt;
}

std::optional<std::size_t> first_talkable_person(const fs::path& game_dir) {
    auto level = load_test_level(game_dir);
    if (!level) return std::nullopt;
    for (std::size_t index = 0; index < level->persons.size(); ++index) {
        if (!level->persons[index].dialog.empty()) return index;
    }
    return std::nullopt;
}

std::optional<std::size_t> level_person_count(const fs::path& game_dir) {
    auto level = load_test_level(game_dir);
    if (!level) return std::nullopt;
    return level->persons.size();
}

void check_dialog_content(const fs::path& dialog_png, const fs::path& menu_png) {
    int dialog_width = 0;
    int dialog_height = 0;
    int menu_width = 0;
    int menu_height = 0;
    const std::vector<unsigned char> dialog = read_png(dialog_png, dialog_width, dialog_height);
    const std::vector<unsigned char> menu = read_png(menu_png, menu_width, menu_height);
    REQUIRE(dialog_width == menu_width);
    REQUIRE(dialog_height == menu_height);

    std::size_t different = 0;
    for (std::size_t index = 0; index < dialog.size(); ++index) {
        if (dialog[index] != menu[index]) ++different;
    }
    CAPTURE(different);
    CHECK(different > 10000);

    const std::size_t bottom_start =
        static_cast<std::size_t>(dialog_height) * 3 / 5;
    const std::size_t bottom_area =
        (static_cast<std::size_t>(dialog_height) - bottom_start) *
        static_cast<std::size_t>(dialog_width);
    const unsigned char* background = dialog.data();
    std::size_t non_background = 0;
    std::set<std::uint32_t> colors;
    for (std::size_t y = bottom_start; y < static_cast<std::size_t>(dialog_height); ++y) {
        for (int x = 0; x < dialog_width; ++x) {
            const unsigned char* pixel =
                dialog.data() + (y * static_cast<std::size_t>(dialog_width) +
                                 static_cast<std::size_t>(x)) *
                                    4;
            const int difference = std::abs(static_cast<int>(pixel[0]) - background[0]) +
                                   std::abs(static_cast<int>(pixel[1]) - background[1]) +
                                   std::abs(static_cast<int>(pixel[2]) - background[2]) +
                                   std::abs(static_cast<int>(pixel[3]) - background[3]);
            if (difference > 12) ++non_background;
            colors.insert((static_cast<std::uint32_t>(pixel[0]) << 16) |
                          (static_cast<std::uint32_t>(pixel[1]) << 8) |
                          static_cast<std::uint32_t>(pixel[2]));
        }
    }
    CAPTURE(non_background);
    CAPTURE(bottom_area);
    CAPTURE(colors.size());
    CHECK(non_background > bottom_area / 10);
    CHECK(colors.size() >= 8);
}
} // namespace

int main(int argc, char** argv) {
    if (std::getenv("GL_GAME_DIR") == nullptr) return 77;
    const bool has_font = font_available();
    doctest::Context context(argc, argv);
    const int result = context.run();
    if (!has_font && result == 0) return 77;
    return result;
}

TEST_CASE("glgame renders the menu into a 1280x720 PNG") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path, true);
    const fs::path png = temp.path / "menu.png";
    const int code = run_glgame(screenshot_arguments(game_dir, config, png, {"--menu"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);
    check_png(png, 1280, 720);
}

TEST_CASE("glgame --list-saves prints the real save slots and exits") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    std::string output;
    const int code =
        run_glgame_capture({"--game-dir", game_dir, "--list-saves"}, output);
    CAPTURE(code);
    CHECK(code == 0);

    const std::vector<std::string> lines = split_lines(output);
    std::size_t slot_lines = 0;
    bool has_slot0 = false;
    bool slot0_has_date = false;
    bool slot0_has_title = false;
    for (const std::string& line : lines) {
        if (line.rfind("slot", 0) != 0) continue;
        ++slot_lines;
        if (line.rfind("slot0\t", 0) == 0) {
            has_slot0 = true;
            slot0_has_date = line.find("\t2022-11-06\t") != std::string::npos;
            slot0_has_title = line.find("\tQuick save\t") != std::string::npos;
        }
    }
    CAPTURE(slot_lines);
    CHECK(slot_lines == 28);
    CHECK(has_slot0);
    CHECK(slot0_has_date);
    CHECK(slot0_has_title);
}

TEST_CASE("glgame renders a dialog into a 1280x720 PNG" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path menu_png = temp.path / "dialog_menu.png";
    const int menu_code = run_glgame(screenshot_arguments(game_dir, config, menu_png, {"--menu"}));
    CAPTURE(menu_code);
    REQUIRE(fs::exists(menu_png));
    CHECK(menu_code == 0);

    const fs::path png = temp.path / "dialog.png";
    const int code = run_glgame(screenshot_arguments(
        game_dir, config, png, {"--dialog", "scripts/dialogs/demon.d1.age.cs"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);
    check_png(png, 1280, 720);
    check_dialog_content(png, menu_png);
}

TEST_CASE("glgame --font changes the rendered dialog" *
          doctest::skip(!font_available() || !alternate_system_font().has_value())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    const std::optional<fs::path> alternate = alternate_system_font();
    REQUIRE(alternate.has_value());

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path default_png = temp.path / "dialog_default.png";
    const fs::path alternate_png = temp.path / "dialog_alternate.png";

    const int default_code = run_glgame(screenshot_arguments(
        game_dir, config, default_png, {"--dialog", "scripts/dialogs/demon.d1.age.cs"}));
    const int alternate_code = run_glgame(screenshot_arguments(
        game_dir, config, alternate_png,
        {"--dialog", "scripts/dialogs/demon.d1.age.cs", "--font", alternate->string()}));
    CAPTURE(default_code);
    CAPTURE(alternate_code);
    REQUIRE(fs::exists(default_png));
    REQUIRE(fs::exists(alternate_png));
    CHECK(default_code == 0);
    CHECK(alternate_code == 0);
    check_png(default_png, 1280, 720);
    check_png(alternate_png, 1280, 720);

    int default_width = 0;
    int default_height = 0;
    int alternate_width = 0;
    int alternate_height = 0;
    const std::vector<unsigned char> default_pixels =
        read_png(default_png, default_width, default_height);
    const std::vector<unsigned char> alternate_pixels =
        read_png(alternate_png, alternate_width, alternate_height);
    REQUIRE(default_width == alternate_width);
    REQUIRE(default_height == alternate_height);
    const std::size_t different = count_differences(default_pixels, alternate_pixels);
    CAPTURE(different);
    CHECK(different > 1000);
}

TEST_CASE("glgame --no-font forces ASCII bitmap text" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path ttf_png = temp.path / "dialog_ttf.png";
    const fs::path ascii_png = temp.path / "dialog_ascii.png";

    const int ttf_code = run_glgame(screenshot_arguments(
        game_dir, config, ttf_png, {"--dialog", "scripts/dialogs/demon.d1.age.cs"}));
    const int ascii_code = run_glgame(screenshot_arguments(
        game_dir, config, ascii_png,
        {"--dialog", "scripts/dialogs/demon.d1.age.cs", "--no-font"}));
    CAPTURE(ttf_code);
    CAPTURE(ascii_code);
    REQUIRE(fs::exists(ttf_png));
    REQUIRE(fs::exists(ascii_png));
    CHECK(ttf_code == 0);
    CHECK(ascii_code == 0);
    check_png(ttf_png, 1280, 720);
    check_png(ascii_png, 1280, 720);

    int ttf_width = 0;
    int ttf_height = 0;
    int ascii_width = 0;
    int ascii_height = 0;
    const std::vector<unsigned char> ttf_pixels = read_png(ttf_png, ttf_width, ttf_height);
    const std::vector<unsigned char> ascii_pixels = read_png(ascii_png, ascii_width, ascii_height);
    REQUIRE(ttf_width == ascii_width);
    REQUIRE(ttf_height == ascii_height);
    const std::size_t different = count_differences(ttf_pixels, ascii_pixels);
    CAPTURE(different);
    CHECK(different > 1000);
}

TEST_CASE("glgame invalid --font falls back to the system font" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path default_png = temp.path / "dialog_default.png";
    const fs::path fallback_png = temp.path / "dialog_fallback.png";
    const fs::path missing_font = temp.path / "missing-font.ttf";

    const int default_code = run_glgame(screenshot_arguments(
        game_dir, config, default_png, {"--dialog", "scripts/dialogs/demon.d1.age.cs"}));
    const int fallback_code = run_glgame(screenshot_arguments(
        game_dir, config, fallback_png,
        {"--dialog", "scripts/dialogs/demon.d1.age.cs", "--font", missing_font.string()}));
    CAPTURE(default_code);
    CAPTURE(fallback_code);
    REQUIRE(fs::exists(default_png));
    REQUIRE(fs::exists(fallback_png));
    CHECK(default_code == 0);
    CHECK(fallback_code == 0);
    check_png(default_png, 1280, 720);
    check_png(fallback_png, 1280, 720);

    int default_width = 0;
    int default_height = 0;
    int fallback_width = 0;
    int fallback_height = 0;
    const std::vector<unsigned char> default_pixels =
        read_png(default_png, default_width, default_height);
    const std::vector<unsigned char> fallback_pixels =
        read_png(fallback_png, fallback_width, fallback_height);
    REQUIRE(default_width == fallback_width);
    REQUIRE(default_height == fallback_height);
    const std::size_t different = count_differences(default_pixels, fallback_pixels);
    CAPTURE(different);
    CHECK(different == 0);
}

TEST_CASE("glgame --dialog forces the console off" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    const std::optional<fs::path> font = system_font_path();
    REQUIRE(font.has_value());

    TempDir temp;
    const fs::path console_off_config = write_test_config(temp.path / "console-off");
    const fs::path console_on_config = write_test_config(temp.path / "console-on", false, true);

    const fs::path off_png = temp.path / "console_off.png";
    const fs::path on_png = temp.path / "console_on.png";
    const std::vector<std::string> extra = {"--dialog", "scripts/dialogs/demon.d1.age.cs",
                                            "--font", font->string()};
    const int off_code = run_glgame(screenshot_arguments(game_dir, console_off_config, off_png, extra));
    const int on_code = run_glgame(screenshot_arguments(game_dir, console_on_config, on_png, extra));
    CAPTURE(off_code);
    CAPTURE(on_code);
    REQUIRE(fs::exists(off_png));
    REQUIRE(fs::exists(on_png));
    CHECK(off_code == 0);
    CHECK(on_code == 0);

    int off_width = 0;
    int off_height = 0;
    int on_width = 0;
    int on_height = 0;
    const std::vector<unsigned char> off = read_png(off_png, off_width, off_height);
    const std::vector<unsigned char> on = read_png(on_png, on_width, on_height);
    REQUIRE(off.size() == on.size());
    std::size_t different = 0;
    for (std::size_t index = 0; index < off.size(); ++index) {
        if (off[index] != on[index]) ++different;
    }
    CAPTURE(different);
    CHECK(different == 0);
}

TEST_CASE("glgame renders a preloaded level into a 1280x720 PNG") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path png = temp.path / "level.png";
    const int code = run_glgame(
        screenshot_arguments(game_dir, config, png, {"--level", "l1_1", "--exec", "gv_pause 1"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);
    check_png(png, 1280, 720);
}

TEST_CASE("glgame --equip-save applies the slot outfit and falls back to naked") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path naked_png = temp.path / "outfit_naked.png";
    const fs::path slot0_png = temp.path / "outfit_slot0.png";
    const fs::path slot2_png = temp.path / "outfit_slot2.png";
    const fs::path invalid_png = temp.path / "outfit_invalid.png";
    const fs::path none_png = temp.path / "outfit_none.png";
    const fs::path pending0_png = temp.path / "outfit_pending0.png";
    const fs::path pending2_png = temp.path / "outfit_pending2.png";
    const fs::path pending_invalid_png = temp.path / "outfit_pending_invalid.png";

    std::string slot0_log;
    std::string invalid_log;
    std::string none_log;
    std::string pending0_log;
    std::string pending_invalid_log;
    const std::vector<std::string> camera = {"--camera-x", "1480", "--camera-y", "0"};
    const auto with_camera = [&](std::vector<std::string> extra) {
        extra.insert(extra.end(), camera.begin(), camera.end());
        return extra;
    };
    const int naked_code = run_glgame(screenshot_arguments(
        game_dir, config, naked_png,
        with_camera({"--level", "l1_1", "--hero-outfit", "none"})));
    const int slot0_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, slot0_png,
                             with_camera({"--level", "l1_1", "--equip-save", "0"})),
        slot0_log);
    const int slot2_code = run_glgame(screenshot_arguments(
        game_dir, config, slot2_png,
        with_camera({"--level", "l1_1", "--equip-save", "2"})));
    const int invalid_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, invalid_png,
                             with_camera({"--level", "l1_1", "--equip-save", "9999"})),
        invalid_log);
    const int none_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, none_png,
                             with_camera({"--level", "l1_1", "--hero-outfit", "none",
                                          "--equip-save", "0"})),
        none_log);
    const int pending0_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, pending0_png,
                             with_camera({"--level", "l1_1", "--equip-save", "0",
                                          "--equip-save-pending"})),
        pending0_log);
    const int pending2_code = run_glgame(screenshot_arguments(
        game_dir, config, pending2_png,
        with_camera({"--level", "l1_1", "--equip-save", "2", "--equip-save-pending"})));
    const int pending_invalid_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, pending_invalid_png,
                             with_camera({"--level", "l1_1", "--equip-save", "9999",
                                          "--equip-save-pending"})),
        pending_invalid_log);

    CAPTURE(naked_code);
    CAPTURE(slot0_code);
    CAPTURE(slot2_code);
    CAPTURE(invalid_code);
    CAPTURE(none_code);
    CAPTURE(pending0_code);
    CAPTURE(pending2_code);
    CAPTURE(pending_invalid_code);
    CAPTURE(slot0_log);
    CAPTURE(invalid_log);
    CAPTURE(none_log);
    CAPTURE(pending0_log);
    CAPTURE(pending_invalid_log);
    REQUIRE(fs::exists(naked_png));
    REQUIRE(fs::exists(slot0_png));
    REQUIRE(fs::exists(slot2_png));
    REQUIRE(fs::exists(invalid_png));
    REQUIRE(fs::exists(none_png));
    REQUIRE(fs::exists(pending0_png));
    REQUIRE(fs::exists(pending2_png));
    REQUIRE(fs::exists(pending_invalid_png));
    CHECK(naked_code == 0);
    CHECK(slot0_code == 0);
    CHECK(slot2_code == 0);
    CHECK(invalid_code == 0);
    CHECK(none_code == 0);
    CHECK(pending0_code == 0);
    CHECK(pending2_code == 0);
    CHECK(pending_invalid_code == 0);
    check_png(naked_png, 1280, 720);
    check_png(slot0_png, 1280, 720);
    check_png(slot2_png, 1280, 720);
    check_png(invalid_png, 1280, 720);
    check_png(none_png, 1280, 720);
    check_png(pending0_png, 1280, 720);
    check_png(pending2_png, 1280, 720);
    check_png(pending_invalid_png, 1280, 720);

    int naked_width = 0;
    int naked_height = 0;
    int slot0_width = 0;
    int slot0_height = 0;
    int slot2_width = 0;
    int slot2_height = 0;
    int invalid_width = 0;
    int invalid_height = 0;
    int none_width = 0;
    int none_height = 0;
    int pending0_width = 0;
    int pending0_height = 0;
    int pending2_width = 0;
    int pending2_height = 0;
    int pending_invalid_width = 0;
    int pending_invalid_height = 0;
    const std::vector<unsigned char> naked =
        read_png(naked_png, naked_width, naked_height);
    const std::vector<unsigned char> slot0 =
        read_png(slot0_png, slot0_width, slot0_height);
    const std::vector<unsigned char> slot2 =
        read_png(slot2_png, slot2_width, slot2_height);
    const std::vector<unsigned char> invalid =
        read_png(invalid_png, invalid_width, invalid_height);
    const std::vector<unsigned char> none = read_png(none_png, none_width, none_height);
    const std::vector<unsigned char> pending0 =
        read_png(pending0_png, pending0_width, pending0_height);
    const std::vector<unsigned char> pending2 =
        read_png(pending2_png, pending2_width, pending2_height);
    const std::vector<unsigned char> pending_invalid =
        read_png(pending_invalid_png, pending_invalid_width, pending_invalid_height);
    REQUIRE(naked.size() == slot0.size());
    REQUIRE(naked.size() == slot2.size());
    REQUIRE(naked.size() == invalid.size());
    REQUIRE(naked.size() == none.size());
    REQUIRE(naked.size() == pending0.size());
    REQUIRE(naked.size() == pending2.size());
    REQUIRE(naked.size() == pending_invalid.size());

    const std::size_t naked_vs_slot0 = count_differences(naked, slot0);
    const std::size_t naked_vs_slot2 = count_differences(naked, slot2);
    const std::size_t slot0_vs_slot2 = count_differences(slot0, slot2);
    const std::size_t naked_vs_invalid = count_differences(naked, invalid);
    const std::size_t naked_vs_none = count_differences(naked, none);
    const std::size_t slot0_vs_pending0 = count_differences(slot0, pending0);
    const std::size_t slot2_vs_pending2 = count_differences(slot2, pending2);
    const std::size_t naked_vs_pending_invalid =
        count_differences(naked, pending_invalid);
    CAPTURE(naked_vs_slot0);
    CAPTURE(naked_vs_slot2);
    CAPTURE(slot0_vs_slot2);
    CAPTURE(naked_vs_invalid);
    CAPTURE(naked_vs_none);
    CAPTURE(slot0_vs_pending0);
    CAPTURE(slot2_vs_pending2);
    CAPTURE(naked_vs_pending_invalid);
    CHECK(naked_vs_slot0 > 1000);
    CHECK(naked_vs_slot2 > 1000);
    CHECK(slot0_vs_slot2 > 1000);
    CHECK(naked_vs_invalid == 0);
    CHECK(naked_vs_none == 0);
    CHECK(slot0_vs_pending0 == 0);
    CHECK(slot2_vs_pending2 == 0);
    CHECK(naked_vs_pending_invalid == 0);
    CHECK(slot0_log.find("outfit: slot 0") != std::string::npos);
    CHECK(invalid_log.find("outfit:") != std::string::npos);
    CHECK(none_log.find("ignored (--hero-outfit none)") != std::string::npos);
    CHECK(pending0_log.find("outfit: slot 0") != std::string::npos);
    CHECK(pending_invalid_log.find("invalid save slot 9999") != std::string::npos);
}

TEST_CASE("glgame --dialog --equip-save populates inventory before the dialog starts" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path png = temp.path / "dialog_inventory.png";
    const std::string dialog_path =
        "scripts/dialogs/l13_1/l13_1.p517_boat_trader.d517.age.cs";
    std::string output;
    const int code = run_glgame_capture(
        screenshot_arguments(game_dir, config, png,
                             {"--dialog", dialog_path, "--equip-save", "0"}),
        output);
    CAPTURE(code);
    CAPTURE(output);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);

    const std::string money_marker = "inventory runtime: Hero money=43875";
    const std::string dialog_marker = "dialog: '" + dialog_path + "'";
    const std::size_t dialog_pos = output.find(dialog_marker);
    REQUIRE(dialog_pos != std::string::npos);
    const std::size_t first_money_pos = output.find(money_marker);
    REQUIRE(first_money_pos != std::string::npos);
    CHECK(first_money_pos < dialog_pos);
    const std::size_t last_runtime_pos =
        output.rfind("inventory runtime: Hero money=", dialog_pos);
    REQUIRE(last_runtime_pos != std::string::npos);
    CHECK(output.compare(last_runtime_pos, money_marker.size(), money_marker) == 0);
}

TEST_CASE("glgame --trade-npc opens the boat trader overlay with real stock" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path png = temp.path / "trade_boat.png";
    std::string output;
    const int code =
        run_glgame_capture(screenshot_arguments(game_dir, config, png,
                                                {"--level", "l13_1", "--equip-save", "0",
                                                 "--trade-npc", "L13_1.P517_Boat_Trader"}),
                           output);
    CAPTURE(code);
    CAPTURE(output);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);
    CHECK(output.find("trade: opened npc='L13_1.P517_Boat_Trader' npc_items=16 "
                      "npc_money=42142") != std::string::npos);
    CHECK(output.find("hero_money=43875") != std::string::npos);

    int width = 0;
    int height = 0;
    const std::vector<unsigned char> pixels = read_png(png, width, height);
    CHECK(width == 1280);
    CHECK(height == 720);
    CHECK_FALSE(pixels.empty());
}

TEST_CASE("glgame --inventory opens the selected save inventory" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path level_png = temp.path / "inventory_level.png";
    const fs::path empty_png = temp.path / "inventory_empty.png";
    const fs::path slot0_png = temp.path / "inventory_slot0.png";
    const fs::path none_png = temp.path / "inventory_none.png";

    std::string slot0_log;
    std::string none_log;
    const int level_code = run_glgame(
        screenshot_arguments(game_dir, config, level_png, {"--level", "l1_1"}));
    const int empty_code = run_glgame(screenshot_arguments(
        game_dir, config, empty_png, {"--level", "l1_1", "--inventory"}));
    const int slot0_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, slot0_png,
                             {"--level", "l1_1", "--equip-save", "0", "--inventory"}),
        slot0_log);
    const int none_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, none_png,
                             {"--level", "l1_1", "--hero-outfit", "none", "--equip-save", "0",
                              "--inventory"}),
        none_log);
    CAPTURE(level_code);
    CAPTURE(empty_code);
    CAPTURE(slot0_code);
    CAPTURE(none_code);
    CAPTURE(slot0_log);
    CAPTURE(none_log);
    REQUIRE(fs::exists(level_png));
    REQUIRE(fs::exists(empty_png));
    REQUIRE(fs::exists(slot0_png));
    REQUIRE(fs::exists(none_png));
    CHECK(level_code == 0);
    CHECK(empty_code == 0);
    CHECK(slot0_code == 0);
    CHECK(none_code == 0);
    CHECK(slot0_log.find("inventory: slot 0") != std::string::npos);
    CHECK(slot0_log.find("items=117") != std::string::npos);
    CHECK(none_log.find("inventory: slot 0") != std::string::npos);
    CHECK(none_log.find("items=117") != std::string::npos);
    check_png(level_png, 1280, 720);
    check_png(empty_png, 1280, 720);
    check_png(slot0_png, 1280, 720);
    check_png(none_png, 1280, 720);

    int level_width = 0;
    int level_height = 0;
    int empty_width = 0;
    int empty_height = 0;
    int slot0_width = 0;
    int slot0_height = 0;
    int none_width = 0;
    int none_height = 0;
    const std::vector<unsigned char> level =
        read_png(level_png, level_width, level_height);
    const std::vector<unsigned char> empty =
        read_png(empty_png, empty_width, empty_height);
    const std::vector<unsigned char> slot0 =
        read_png(slot0_png, slot0_width, slot0_height);
    const std::vector<unsigned char> none = read_png(none_png, none_width, none_height);
    REQUIRE(level_width == empty_width);
    REQUIRE(level_height == empty_height);
    REQUIRE(level_width == slot0_width);
    REQUIRE(level_height == slot0_height);
    REQUIRE(level_width == none_width);
    REQUIRE(level_height == none_height);

    const std::size_t level_vs_empty = count_differences(level, empty);
    const std::size_t empty_vs_slot0 = count_differences(empty, slot0);
    const std::size_t empty_vs_none = count_differences(empty, none);
    CAPTURE(level_vs_empty);
    CAPTURE(empty_vs_slot0);
    CAPTURE(empty_vs_none);
    CHECK(level_vs_empty > 1000);
    CHECK(empty_vs_slot0 > 1000);
    CHECK(empty_vs_none > 1000);
}

TEST_CASE("glgame --inventory with an invalid save opens an empty overlay" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path level_png = temp.path / "inventory_invalid_level.png";
    const fs::path empty_png = temp.path / "inventory_invalid_empty.png";
    const fs::path invalid_png = temp.path / "inventory_invalid.png";
    const fs::path hook_open_png = temp.path / "inventory_hook_open.png";
    const fs::path hook_closed_png = temp.path / "inventory_hook_closed.png";

    std::string invalid_log;
    const int level_code = run_glgame(
        screenshot_arguments(game_dir, config, level_png, {"--level", "l1_1"}));
    const int empty_code = run_glgame(screenshot_arguments(
        game_dir, config, empty_png, {"--level", "l1_1", "--inventory"}));
    const int invalid_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, invalid_png,
                             {"--level", "l1_1", "--equip-save", "9999", "--inventory"}),
        invalid_log);
    const int hook_open_code = run_glgame(screenshot_arguments(
        game_dir, config, hook_open_png, {"--level", "l1_1", "--exec", "inventory"}));
    const int hook_closed_code = run_glgame(screenshot_arguments(
        game_dir, config, hook_closed_png,
        {"--level", "l1_1", "--exec", "inventory", "--exec", "inventory"}));
    CAPTURE(level_code);
    CAPTURE(empty_code);
    CAPTURE(invalid_code);
    CAPTURE(hook_open_code);
    CAPTURE(hook_closed_code);
    CAPTURE(invalid_log);
    REQUIRE(fs::exists(level_png));
    REQUIRE(fs::exists(empty_png));
    REQUIRE(fs::exists(invalid_png));
    REQUIRE(fs::exists(hook_open_png));
    REQUIRE(fs::exists(hook_closed_png));
    CHECK(level_code == 0);
    CHECK(empty_code == 0);
    CHECK(invalid_code == 0);
    CHECK(hook_open_code == 0);
    CHECK(hook_closed_code == 0);
    CHECK(invalid_log.find("invalid save slot 9999") != std::string::npos);
    check_png(level_png, 1280, 720);
    check_png(empty_png, 1280, 720);
    check_png(invalid_png, 1280, 720);
    check_png(hook_open_png, 1280, 720);
    check_png(hook_closed_png, 1280, 720);

    int level_width = 0;
    int level_height = 0;
    int empty_width = 0;
    int empty_height = 0;
    int invalid_width = 0;
    int invalid_height = 0;
    int hook_open_width = 0;
    int hook_open_height = 0;
    int hook_closed_width = 0;
    int hook_closed_height = 0;
    const std::vector<unsigned char> level =
        read_png(level_png, level_width, level_height);
    const std::vector<unsigned char> empty =
        read_png(empty_png, empty_width, empty_height);
    const std::vector<unsigned char> invalid =
        read_png(invalid_png, invalid_width, invalid_height);
    const std::vector<unsigned char> hook_open =
        read_png(hook_open_png, hook_open_width, hook_open_height);
    const std::vector<unsigned char> hook_closed =
        read_png(hook_closed_png, hook_closed_width, hook_closed_height);
    REQUIRE(level_width == empty_width);
    REQUIRE(level_height == empty_height);
    REQUIRE(level_width == invalid_width);
    REQUIRE(level_height == invalid_height);
    REQUIRE(level_width == hook_open_width);
    REQUIRE(level_height == hook_open_height);
    REQUIRE(level_width == hook_closed_width);
    REQUIRE(level_height == hook_closed_height);

    const std::size_t level_vs_invalid = count_differences(level, invalid);
    const std::size_t canvas_differences =
        count_region_differences(empty, invalid, empty_width, empty_height, 160, 0, 960, 720);
    const std::size_t empty_vs_hook_open = count_differences(empty, hook_open);
    const std::size_t level_vs_hook_closed = count_differences(level, hook_closed);
    CAPTURE(level_vs_invalid);
    CAPTURE(canvas_differences);
    CAPTURE(empty_vs_hook_open);
    CAPTURE(level_vs_hook_closed);
    CHECK(level_vs_invalid > 1000);
    CHECK(canvas_differences == 0);
    CHECK(empty_vs_hook_open == 0);
    CHECK(level_vs_hook_closed == 0);
}

TEST_CASE("glgame selected inventory renders panel text at multiple viewports" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    struct Viewport {
        int width;
        int height;
    };
    const Viewport viewports[] = {{1024, 768}, {1280, 720}, {600, 800}};
    for (const Viewport& viewport : viewports) {
        CAPTURE(viewport.width);
        CAPTURE(viewport.height);
        const std::string width = std::to_string(viewport.width);
        const std::string height = std::to_string(viewport.height);
        const fs::path empty_png = temp.path / ("panel_empty_" + width + "x" + height + ".png");
        const fs::path first_png = temp.path / ("panel_first_" + width + "x" + height + ".png");
        const fs::path second_png = temp.path / ("panel_second_" + width + "x" + height + ".png");

        const std::vector<std::string> base = {
            "--level", "l1_1", "--equip-save", "0", "--inventory",
            "--width", width, "--height", height};
        std::vector<std::string> first = base;
        first.insert(first.end(), {"--inventory-select", "0"});
        std::vector<std::string> second = base;
        second.insert(second.end(), {"--inventory-select", "1"});

        const int empty_code = run_glgame(screenshot_arguments(game_dir, config, empty_png, base));
        const int first_code = run_glgame(screenshot_arguments(game_dir, config, first_png, first));
        const int second_code = run_glgame(screenshot_arguments(game_dir, config, second_png,
                                                               second));
        CAPTURE(empty_code);
        CAPTURE(first_code);
        CAPTURE(second_code);
        REQUIRE(fs::exists(empty_png));
        REQUIRE(fs::exists(first_png));
        REQUIRE(fs::exists(second_png));
        CHECK(empty_code == 0);
        CHECK(first_code == 0);
        CHECK(second_code == 0);
        check_png(empty_png, viewport.width, viewport.height);
        check_png(first_png, viewport.width, viewport.height);
        check_png(second_png, viewport.width, viewport.height);

        int empty_width = 0;
        int empty_height = 0;
        int first_width = 0;
        int first_height = 0;
        int second_width = 0;
        int second_height = 0;
        const std::vector<unsigned char> empty =
            read_png(empty_png, empty_width, empty_height);
        const std::vector<unsigned char> selected =
            read_png(first_png, first_width, first_height);
        const std::vector<unsigned char> other =
            read_png(second_png, second_width, second_height);
        REQUIRE(empty_width == viewport.width);
        REQUIRE(empty_height == viewport.height);
        REQUIRE(first_width == viewport.width);
        REQUIRE(first_height == viewport.height);
        REQUIRE(second_width == viewport.width);
        REQUIRE(second_height == viewport.height);

        const PixelRect panel = inventory_panel_rect(viewport.width, viewport.height);
        const std::size_t empty_total = count_region_differences(
            empty, selected, viewport.width, viewport.height, 0, 0, viewport.width,
            viewport.height);
        const std::size_t empty_panel = count_region_differences(
            empty, selected, viewport.width, viewport.height, panel.x, panel.y, panel.width,
            panel.height);
        const std::size_t other_total = count_region_differences(
            selected, other, viewport.width, viewport.height, 0, 0, viewport.width,
            viewport.height);
        const std::size_t other_panel = count_region_differences(
            selected, other, viewport.width, viewport.height, panel.x, panel.y, panel.width,
            panel.height);
        CAPTURE(empty_total);
        CAPTURE(empty_panel);
        CAPTURE(other_total);
        CAPTURE(other_panel);
        CHECK(empty_panel > 100);
        CHECK(empty_total == empty_panel);
        CHECK(other_panel > 0);
        CHECK(other_total == other_panel);
    }
}

TEST_CASE("glgame --no-font still renders the inventory panel") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path empty_png = temp.path / "no_font_panel_empty.png";
    const fs::path selected_png = temp.path / "no_font_panel_selected.png";
    const std::vector<std::string> base = {"--level", "l1_1", "--equip-save", "0", "--inventory",
                                           "--no-font", "--width", "1024", "--height", "768"};
    std::vector<std::string> selected = base;
    selected.insert(selected.end(), {"--inventory-select", "0"});

    const int empty_code =
        run_glgame(screenshot_arguments(game_dir, config, empty_png, base));
    const int selected_code =
        run_glgame(screenshot_arguments(game_dir, config, selected_png, selected));
    CAPTURE(empty_code);
    CAPTURE(selected_code);
    REQUIRE(fs::exists(empty_png));
    REQUIRE(fs::exists(selected_png));
    CHECK(empty_code == 0);
    CHECK(selected_code == 0);
    check_png(empty_png, 1024, 768);
    check_png(selected_png, 1024, 768);

    int empty_width = 0;
    int empty_height = 0;
    int selected_width = 0;
    int selected_height = 0;
    const std::vector<unsigned char> empty =
        read_png(empty_png, empty_width, empty_height);
    const std::vector<unsigned char> chosen =
        read_png(selected_png, selected_width, selected_height);
    REQUIRE(empty_width == 1024);
    REQUIRE(empty_height == 768);
    REQUIRE(selected_width == 1024);
    REQUIRE(selected_height == 768);
    const PixelRect panel = inventory_panel_rect(1024, 768);
    CHECK(count_region_differences(empty, chosen, 1024, 768, panel.x, panel.y, panel.width,
                                   panel.height) > 100);
}

TEST_CASE("glgame --click-npc opens the dialog overlay over the level" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    const std::optional<std::size_t> talkable = first_talkable_person(game_dir);
    REQUIRE(talkable.has_value());

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path baseline_png = temp.path / "click_baseline.png";
    const fs::path dialog_png = temp.path / "click_dialog.png";

    const int baseline_code = run_glgame(
        screenshot_arguments(game_dir, config, baseline_png, {"--level", "l1_1"}));
    std::string output;
    const int dialog_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, dialog_png,
                             {"--level", "l1_1", "--click-npc", std::to_string(*talkable)}),
        output);
    CAPTURE(baseline_code);
    CAPTURE(dialog_code);
    CAPTURE(output);
    REQUIRE(fs::exists(baseline_png));
    REQUIRE(fs::exists(dialog_png));
    CHECK(baseline_code == 0);
    CHECK(dialog_code == 0);
    CHECK(output.find("dialog: '") != std::string::npos);
    check_png(baseline_png, 1280, 720);
    check_png(dialog_png, 1280, 720);

    int baseline_width = 0;
    int baseline_height = 0;
    int dialog_width = 0;
    int dialog_height = 0;
    const std::vector<unsigned char> baseline =
        read_png(baseline_png, baseline_width, baseline_height);
    const std::vector<unsigned char> dialog = read_png(dialog_png, dialog_width, dialog_height);
    REQUIRE(baseline_width == dialog_width);
    REQUIRE(baseline_height == dialog_height);
    const std::size_t different =
        count_bottom_differences(baseline, dialog, dialog_width, dialog_height, 220);
    CAPTURE(different);
    CHECK(different > 1280u * 220u / 10u);
}

TEST_CASE("glgame --click-npc with an out-of-range index opens no overlay" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    const std::optional<std::size_t> count = level_person_count(game_dir);
    REQUIRE(count.has_value());

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path baseline_png = temp.path / "invalid_baseline.png";
    const fs::path click_png = temp.path / "invalid_click.png";

    const int baseline_code = run_glgame(
        screenshot_arguments(game_dir, config, baseline_png, {"--level", "l1_1"}));
    std::string output;
    const int click_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, click_png,
                             {"--level", "l1_1", "--click-npc", std::to_string(*count)}),
        output);
    CAPTURE(baseline_code);
    CAPTURE(click_code);
    CAPTURE(output);
    REQUIRE(fs::exists(baseline_png));
    REQUIRE(fs::exists(click_png));
    CHECK(baseline_code == 0);
    CHECK(click_code == 0);
    CHECK(output.find("dialog: '") == std::string::npos);
    CHECK(output.find("invalid person index") != std::string::npos);
    check_png(click_png, 1280, 720);

    int baseline_width = 0;
    int baseline_height = 0;
    int click_width = 0;
    int click_height = 0;
    const std::vector<unsigned char> baseline =
        read_png(baseline_png, baseline_width, baseline_height);
    const std::vector<unsigned char> click = read_png(click_png, click_width, click_height);
    REQUIRE(baseline_width == click_width);
    REQUIRE(baseline_height == click_height);
    CHECK(count_differences(baseline, click) == 0);
}

TEST_CASE("glgame clicks the real l1_1 P621 circus worker and opens its dialog" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    const std::optional<std::size_t> person =
        level_person_index(game_dir, "L1.P621_Svet_3CircusWorkerV");
    REQUIRE(person.has_value());
    CAPTURE(*person);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path baseline_png = temp.path / "p621_baseline.png";
    const fs::path dialog_png = temp.path / "p621_dialog.png";

    const int baseline_code = run_glgame(
        screenshot_arguments(game_dir, config, baseline_png, {"--level", "l1_1"}));
    std::string output;
    const int dialog_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, dialog_png,
                             {"--level", "l1_1", "--click-npc", std::to_string(*person)}),
        output);
    CAPTURE(baseline_code);
    CAPTURE(dialog_code);
    CAPTURE(output);
    REQUIRE(fs::exists(baseline_png));
    REQUIRE(fs::exists(dialog_png));
    CHECK(baseline_code == 0);
    CHECK(dialog_code == 0);
    CHECK(output.find("dialog: 'scripts/dialogs/l1_1/"
                      "l1.p621_svet_3circusworkerv.d621.age.cs'") != std::string::npos);
    CHECK(output.find("speaker='L1.P621_Svet_3CircusWorkerV'") != std::string::npos);
    CHECK(output.find("answers=2") != std::string::npos);
    CHECK(output.find(" text_len=") != std::string::npos);
    CHECK(output.find(" text_len=0") == std::string::npos);
    check_png(baseline_png, 1280, 720);
    check_png(dialog_png, 1280, 720);

    int baseline_width = 0;
    int baseline_height = 0;
    int dialog_width = 0;
    int dialog_height = 0;
    const std::vector<unsigned char> baseline =
        read_png(baseline_png, baseline_width, baseline_height);
    const std::vector<unsigned char> dialog = read_png(dialog_png, dialog_width, dialog_height);
    REQUIRE(baseline_width == dialog_width);
    REQUIRE(baseline_height == dialog_height);
    const std::size_t different =
        count_bottom_differences(baseline, dialog, dialog_width, dialog_height, 220);
    CAPTURE(different);
    CHECK(different > 1280u * 220u / 10u);
}

TEST_CASE("glgame clicks the real l1_1 moving NPCs and opens their dialogs" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const std::vector<std::string> names = {
        "L0.P678_citizen4",
        "L0.P7_Kolbasens_Rodstvennik1",
    };
    for (std::size_t i = 0; i < names.size(); ++i) {
        CAPTURE(names[i]);
        const std::optional<std::size_t> person = level_person_index(game_dir, names[i]);
        REQUIRE(person.has_value());
        const fs::path png = temp.path / ("moving_" + std::to_string(i) + ".png");
        std::string output;
        const int code = run_glgame_capture(
            screenshot_arguments(game_dir, config, png,
                                 {"--level", "l1_1", "--click-npc", std::to_string(*person)}),
            output);
        CAPTURE(code);
        CAPTURE(output);
        REQUIRE(fs::exists(png));
        CHECK(code == 0);
        CHECK(output.find("speaker='" + names[i] + "'") != std::string::npos);
        CHECK(output.find("dialog: 'scripts/dialogs/l1_1_nap/"
                          "l1_svet_malepeasant1.age.cs'") != std::string::npos);
        CHECK(output.find(" text_len=") != std::string::npos);
        CHECK(output.find(" text_len=0") == std::string::npos);
        check_png(png, 1280, 720);
    }
}

TEST_CASE("glgame --click-world sets the hero goal without opening a dialog" *
          doctest::skip(!font_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path png = temp.path / "click_world.png";

    std::string output;
    const int code = run_glgame_capture(
        screenshot_arguments(game_dir, config, png,
                             {"--level", "l1_1", "--click-world", "218", "50"}),
        output);
    CAPTURE(code);
    CAPTURE(output);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);
    check_png(png, 1280, 720);
    CHECK(output.find("dialog: '") == std::string::npos);
    CHECK(output.find("cannot path to the clicked world tile") == std::string::npos);
}

TEST_CASE("glgame --no-persons hides NPC sprites") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path normal_png = temp.path / "level_persons.png";
    const fs::path no_persons_png = temp.path / "level_no_persons.png";

    const int normal_code = run_glgame(
        screenshot_arguments(game_dir, config, normal_png, {"--level", "l1_1"}));
    const int no_persons_code = run_glgame(screenshot_arguments(
        game_dir, config, no_persons_png, {"--level", "l1_1", "--no-persons"}));
    CAPTURE(normal_code);
    CAPTURE(no_persons_code);
    REQUIRE(fs::exists(normal_png));
    REQUIRE(fs::exists(no_persons_png));
    CHECK(normal_code == 0);
    CHECK(no_persons_code == 0);
    check_png(normal_png, 1280, 720);
    check_png(no_persons_png, 1280, 720);

    int normal_width = 0;
    int normal_height = 0;
    int plain_width = 0;
    int plain_height = 0;
    const std::vector<unsigned char> normal = read_png(normal_png, normal_width, normal_height);
    const std::vector<unsigned char> plain =
        read_png(no_persons_png, plain_width, plain_height);
    REQUIRE(normal_width == plain_width);
    REQUIRE(normal_height == plain_height);
    const std::size_t different = count_differences(normal, plain);
    CAPTURE(different);
    CHECK(different > 1000);
}

TEST_CASE("glgame NPCs move between t=0 and t=20s") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);
    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path first_png = temp.path / "move_first.png";
    const fs::path later_png = temp.path / "move_later.png";
    const fs::path first_plain_png = temp.path / "move_first_plain.png";
    const fs::path later_plain_png = temp.path / "move_later_plain.png";

    std::string log;
    const int first_code = run_glgame_capture(
        screenshot_arguments(game_dir, config, first_png,
                             {"--level", "l1_1", "--sim-ms", "0"}),
        log);
    // Loot now consumes the shared RNG before movement (research §15.3); at
    // seed 1 the 20 s window keeps the unchanged >1000 presence-diff threshold.
    const int later_code = run_glgame(screenshot_arguments(
        game_dir, config, later_png, {"--level", "l1_1", "--sim-ms", "20000"}));
    const int first_plain_code = run_glgame(screenshot_arguments(
        game_dir, config, first_plain_png,
        {"--level", "l1_1", "--sim-ms", "0", "--no-persons"}));
    const int later_plain_code = run_glgame(screenshot_arguments(
        game_dir, config, later_plain_png,
        {"--level", "l1_1", "--sim-ms", "20000", "--no-persons"}));
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

TEST_CASE("glgame tolerates help and unknown commands from --exec") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path png = temp.path / "exec.png";
    const int code = run_glgame(screenshot_arguments(
        game_dir, config, png, {"--menu", "--exec", "help", "--exec", "unknown"}));
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
        game_dir, config, png, {"--menu", "--exec", "cl_username TestUser"}));
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

TEST_CASE("glgame starts from defaults when --config is missing and creates it on save") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = temp.path / "missing" / "GoldenLand.ini";
    const fs::path png = temp.path / "missing.png";
    const int code = run_glgame(screenshot_arguments(
        game_dir, config, png, {"--menu", "--exec", "cl_username Missing"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);
    check_png(png, 1280, 720);

    REQUIRE(fs::exists(config));
    const std::vector<std::string> lines = split_lines(read_file(config));
    CHECK(setting_value(lines, "cl_username") == "Missing");
}

TEST_CASE("glgame persists r_resolution through ServerCore") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path png = temp.path / "resolution.png";
    const int code = run_glgame(screenshot_arguments(
        game_dir, config, png, {"--menu", "--exec", "r_resolution 800 600"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);

    const std::vector<std::string> lines = split_lines(read_file(config));
    CHECK(has_line(lines, "r_resolution 800 600"));
}

TEST_CASE("glgame persists gv_pause and r_windowed") {
    const char* game_dir = std::getenv("GL_GAME_DIR");
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path config = write_test_config(temp.path);
    const fs::path png = temp.path / "persist.png";
    const int code = run_glgame(screenshot_arguments(
        game_dir, config, png,
        {"--menu", "--exec", "gv_pause 1", "--exec", "r_windowed 0"}));
    CAPTURE(code);
    REQUIRE(fs::exists(png));
    CHECK(code == 0);

    const std::vector<std::string> lines = split_lines(read_file(config));
    CHECK(setting_value(lines, "gv_pause") == "1");
    CHECK(setting_value(lines, "r_windowed") == "0");
}
