#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/render/font.hpp>
#include <stb_image.h>

#include <atomic>
#include <chrono>
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
    FILE* pipe = _popen(command.c_str(), "rb");
#else
    std::string command = "\"" + g_glgame_exe.string() + "\"";
    for (const auto& arg : arguments) command += " \"" + arg + "\"";
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
        "--height",   "720"};
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
