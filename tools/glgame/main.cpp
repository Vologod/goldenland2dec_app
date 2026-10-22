#include <gl/app/level_screen.hpp>
#include <gl/assets/cs.hpp>
#include <gl/assets/log.hpp>
#include <gl/assets/sdb.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/core/command.hpp>
#include <gl/core/config.hpp>
#include <gl/core/console.hpp>
#include <gl/core/save_manager.hpp>
#include <gl/core/server.hpp>
#include <gl/core/state.hpp>
#include <gl/platform/sdl_context.hpp>
#include <gl/render/font.hpp>
#include <gl/render/text.hpp>
#include <gl/script/dialog_session.hpp>
#include <gl/script/globals.hpp>
#include <gl/script/host.hpp>
#include <gl/script/script_vm.hpp>

#include <SDL.h>
#include <stb_image_write.h>

#include <algorithm>
#include <charconv>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 720;
constexpr int kMaxDimension = 16384;
constexpr std::uint64_t kMaxSimMs = 86'400'000;
constexpr std::string_view kNewGameLevel = "l1_1";
constexpr std::string_view kTestDialogScript = "scripts/dialogs/demon.d1.age.cs";
constexpr std::string_view kDialogGlobals = "scripts/dialogs_special/zlato_vars.scr";
constexpr std::string_view kDialogSdb = "sdb/dialogs/dialogsphrases.sdb";
constexpr float kZoomStep = 1.1f;
constexpr float kScale = 2.0f;
constexpr float kTitleScale = 3.0f;
constexpr float kFontPixelHeight = 20.0f;
constexpr std::size_t kConsoleOutputLines = 10;
constexpr float kSaveRowHeight = 17.0f;
constexpr float kDialogMargin = 32.0f;
constexpr float kDialogAnswerGap = 8.0f;
constexpr int kBaseMenuItems = 3;

struct Options {
    fs::path game_dir;
    fs::path config_path;
    std::string level = std::string(kNewGameLevel);
    bool has_level = false;
    bool menu = false;
    bool list_saves = false;
    int width = kDefaultWidth;
    int height = kDefaultHeight;
    bool width_set = false;
    bool height_set = false;
    fs::path screenshot;
    fs::path font;
    bool font_set = false;
    bool no_font = false;
    bool no_persons = false;
    bool walkable_overlay = false;
    std::uint64_t sim_ms = 0;
    std::string dialog;
    bool has_dialog = false;
    std::vector<std::string> exec;
};

enum class ParseStatus { ok, help, error };

void print_usage() {
    std::puts(
        "Usage: glgame --game-dir <dir> [options]\n"
        "\n"
        "Options:\n"
        "  --game-dir <dir>     Game installation directory (required)\n"
        "  --config <ini>       Config file (default: <game-dir>/GoldenLand.ini)\n"
        "  --level <name>       Level to start or load (default: l1_1)\n"
        "  --width <px>         Window width (default: 1280)\n"
        "  --height <px>        Window height (default: 720)\n"
        "  --menu               Force starting in the main menu\n"
        "  --dialog <path.cs>   Open a dialog script screen directly (virtual path)\n"
        "  --font <ttf>         TrueType font for dialog text (default: system font)\n"
        "  --no-font            Force ASCII bitmap text (dev/debug, overrides --font)\n"
        "  --no-persons         Skip NPC sprites on the level\n"
        "  --walkable-overlay   Draw blocked tiles in red on the level\n"
        "  --sim-ms <ms>        Simulation time for screenshots (ignored interactively)\n"
        "  --list-saves         Print save slots as tab-separated lines and exit\n"
        "  --screenshot <png>   Render the current state, save PNG and exit\n"
        "  --exec \"<command>\"   Run a console command after boot (repeatable)\n"
        "  -h, --help           Show this help and exit\n"
        "\n"
        "Console commands include: help, exit, saves.\n"
        "Exit codes: 0 success, 1 arguments, 2 I/O, 3 format/load.");
}

fs::path path_from_utf8(std::string_view text) {
    std::u8string utf8;
    utf8.reserve(text.size());
    for (const char c : text) utf8.push_back(static_cast<char8_t>(c));
    return fs::path(utf8);
}

bool parse_int(std::string_view text, int& value) {
    if (text.empty()) return false;
    int parsed = 0;
    const char* const end = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(text.data(), end, parsed);
    if (error != std::errc{} || ptr != end) return false;
    value = parsed;
    return true;
}

bool parse_dimension(std::string_view text, int& value) {
    return parse_int(text, value) && value > 0 && value <= kMaxDimension;
}

bool parse_milliseconds(std::string_view text, std::uint64_t& value) {
    if (text.empty()) return false;
    std::uint64_t parsed = 0;
    const char* const end = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(text.data(), end, parsed);
    if (error != std::errc{} || ptr != end || parsed > kMaxSimMs) return false;
    value = parsed;
    return true;
}

struct Resolution {
    int width = 0;
    int height = 0;
    bool valid = false;
};

Resolution parse_resolution(const std::optional<std::string>& value) {
    if (!value) return {};
    std::istringstream stream(*value);
    int width = 0;
    int height = 0;
    std::string extra;
    if (!(stream >> width >> height)) return {};
    if (stream >> extra) return {};
    if (width <= 0 || height <= 0 || width > kMaxDimension || height > kMaxDimension) return {};
    return Resolution{width, height, true};
}

ParseStatus parse_args(int argc, char** argv, Options& options) {
    bool has_game_dir = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return ParseStatus::help;
        }

        const auto next_value = [&](std::string_view name, std::string_view& value) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "glgame: missing value for %.*s\n",
                             static_cast<int>(name.size()), name.data());
                return false;
            }
            value = argv[++i];
            return true;
        };

        std::string_view value;
        if (arg == "--game-dir") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value.empty()) {
                std::fputs("glgame: --game-dir must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.game_dir = path_from_utf8(value);
            has_game_dir = true;
        } else if (arg == "--config") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value.empty()) {
                std::fputs("glgame: --config must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.config_path = path_from_utf8(value);
        } else if (arg == "--level") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value.empty()) {
                std::fputs("glgame: --level must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.level = std::string(value);
            options.has_level = true;
        } else if (arg == "--width") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (!parse_dimension(value, options.width)) {
                std::fprintf(stderr, "glgame: invalid --width: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.width_set = true;
        } else if (arg == "--height") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (!parse_dimension(value, options.height)) {
                std::fprintf(stderr, "glgame: invalid --height: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.height_set = true;
        } else if (arg == "--menu") {
            options.menu = true;
        } else if (arg == "--dialog") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value.empty()) {
                std::fputs("glgame: --dialog must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.dialog = std::string(value);
            options.has_dialog = true;
        } else if (arg == "--font") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value.empty()) {
                std::fputs("glgame: --font must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.font = path_from_utf8(value);
            options.font_set = true;
        } else if (arg == "--no-font") {
            options.no_font = true;
        } else if (arg == "--no-persons") {
            options.no_persons = true;
        } else if (arg == "--walkable-overlay") {
            options.walkable_overlay = true;
        } else if (arg == "--sim-ms") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (!parse_milliseconds(value, options.sim_ms)) {
                std::fprintf(stderr, "glgame: invalid --sim-ms: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
        } else if (arg == "--list-saves") {
            options.list_saves = true;
        } else if (arg == "--screenshot") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value.empty()) {
                std::fputs("glgame: --screenshot must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.screenshot = path_from_utf8(value);
        } else if (arg == "--exec") {
            if (!next_value(arg, value)) return ParseStatus::error;
            options.exec.emplace_back(value);
        } else {
            std::fprintf(stderr, "glgame: unknown argument: %s\n", argv[i]);
            print_usage();
            return ParseStatus::error;
        }
    }

    if (!has_game_dir) {
        std::fputs("glgame: --game-dir is required\n", stderr);
        print_usage();
        return ParseStatus::error;
    }
    return ParseStatus::ok;
}

int exit_code_for(const gl::Error& error) {
    switch (error.code) {
        case gl::ErrorCode::argument:
            return 1;
        case gl::ErrorCode::io:
            return 2;
        case gl::ErrorCode::format:
        case gl::ErrorCode::unsupported:
            return 3;
    }
    return 3;
}

bool is_blank(std::string_view text) {
    for (const char c : text) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
    }
    return true;
}

const char* state_name(gl::core::AppState state) {
    switch (state) {
        case gl::core::AppState::boot:
            return "boot";
        case gl::core::AppState::menu:
            return "menu";
        case gl::core::AppState::level:
            return "level";
        case gl::core::AppState::exit:
            return "exit";
    }
    return "unknown";
}

std::string format_save_date(const gl::assets::SaveHeader& header) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u", static_cast<unsigned>(header.year),
                  static_cast<unsigned>(header.month), static_cast<unsigned>(header.day));
    return buffer;
}

std::string display_title(const gl::assets::SaveHeader& header) {
    return header.title.empty() ? std::string("(no title)") : header.title;
}

std::string format_save_row(const gl::core::SaveSlotInfo& slot) {
    const std::uint64_t kilobytes = (slot.file_size + 1023) / 1024;
    return "Slot " + std::to_string(slot.slot) + "  " + format_save_date(slot.header) + "  " +
           display_title(slot.header) + "  (" + std::to_string(kilobytes) + " KB)";
}

std::string format_save_list_line(const gl::core::SaveSlotInfo& slot) {
    return "slot" + std::to_string(slot.slot) + "\t" + format_save_date(slot.header) + "\t" +
           display_title(slot.header) + "\t" + std::to_string(slot.file_size);
}

bool is_displayable(const gl::core::SaveSlotInfo& slot) {
    return slot.slot >= 0 && slot.valid;
}

std::string summarize_output(std::string_view line, std::string_view text) {
    if (text.find('\n') == std::string_view::npos) return std::string(text);
    const std::size_t lines =
        static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')) + 1;
    const std::size_t end = line.find_first_of(" \t");
    const std::string command(line.substr(0, end));
    if (command == "saves") return "saves: " + std::to_string(lines) + " slot(s)";
    return command + ": " + std::to_string(lines) + " line(s)";
}

void erase_last_utf8(std::string& text) {
    if (text.empty()) return;
    std::size_t index = text.size() - 1;
    while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) --index;
    text.erase(index);
}

struct DialogContext {
    gl::assets::Sdb sdb;
    gl::script::DefaultScriptHost host;
    std::unique_ptr<gl::script::ScriptVm> vm;
    std::unique_ptr<gl::script::DialogSession> session;
    std::string path;
};

class Game {
  public:
    Game(Options options, gl::core::Config config, fs::path config_path,
         gl::assets::GameVfs& vfs, gl::platform::Window& window)
        : options_(std::move(options)), config_(std::move(config)),
          config_path_(std::move(config_path)), vfs_(vfs), window_(window) {
        level_name_ = options_.level;
        screenshot_mode_ = !options_.screenshot.empty();
        register_commands();
    }

    int boot() {
        gl::log(gl::LogLevel::info,
                "glgame: config '" + config_path_.string() + "' (" +
                    std::to_string(config_.raw_lines().size()) + " lines)");
        const std::optional<std::string> username = config_.get("cl_username");
        gl::log(gl::LogLevel::info,
                "glgame: user '" + username.value_or("") + "', windowed=" +
                    std::string(config_.get_bool("r_windowed", true) ? "1" : "0"));

        auto saves = gl::core::SaveManager::scan(options_.game_dir / "Saves");
        if (!saves) {
            gl::log(gl::LogLevel::warning, "save: " + saves.error().message);
        } else {
            saves_ = std::move(saves.value());
        }
        rebuild_display_saves();
        gl::log(gl::LogLevel::info,
                "glgame: save slots=" + std::to_string(saves_.slots().size()) + ", valid=" +
                    std::to_string(display_saves_.size()));

        test_dialog_available_ = vfs_.exists(kTestDialogScript);
        load_font();

        if (!state_.transition(gl::core::AppState::menu)) return 3;

        apply_initial_server_config();
        if (config_.get_bool("d_console", false)) set_console_visible(true);
        if (options_.has_dialog && console_visible_) {
            set_console_visible(false);
            gl::log(gl::LogLevel::info, "dialog: console disabled for --dialog");
        }

        if (options_.has_dialog) {
            const gl::Result<void> opened = open_dialog(options_.dialog);
            if (!opened) return exit_code_for(opened.error());
            dialog_launched_from_cli_ = true;
        } else if (options_.has_level && !options_.menu) {
            if (!load_level(level_name_)) return 3;
            if (!state_.transition(gl::core::AppState::level)) return 3;
        }

        update_viewport();

        for (const std::string& command : options_.exec) {
            execute_line(command, false);
        }

        gl::log(gl::LogLevel::info,
                "glgame: boot complete, state=" + std::string(state_name(state_.state())) +
                    ", commands=" + std::to_string(registry_.commands().size()) +
                    ", archives=" + std::to_string(vfs_.archives().size()) +
                    ", console=" + std::string(console_visible_ ? "1" : "0"));
        return 0;
    }

    int screenshot(const fs::path& path) {
        SDL_Renderer* renderer = window_.renderer();
        int width = 0;
        int height = 0;
        if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 ||
            height <= 0) {
            std::fprintf(stderr, "glgame: cannot query renderer output size: %s\n",
                         SDL_GetError());
            return 2;
        }

        const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) *
                                          static_cast<std::uint64_t>(height);
        if (pixel_count > static_cast<std::uint64_t>(INT_MAX) || width > INT_MAX / 4) {
            std::fprintf(stderr, "glgame: renderer output too large: %dx%d\n", width, height);
            return 3;
        }

        const int pitch = width * 4;
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(pixel_count) * 4);
        render(0, options_.sim_ms);
        if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels.data(),
                                 pitch) != 0) {
            std::fprintf(stderr, "glgame: SDL_RenderReadPixels failed: %s\n", SDL_GetError());
            return 2;
        }
        SDL_RenderPresent(renderer);

        if (stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), pitch) == 0) {
            std::fprintf(stderr, "glgame: cannot write screenshot: %s\n",
                         path.string().c_str());
            return 2;
        }

        gl::log(gl::LogLevel::info,
                "saved screenshot " + path.string() + " (" + std::to_string(width) + "x" +
                    std::to_string(height) + ")");
        if (!save_config()) return 2;
        return 0;
    }

    int run_interactive() {
        apply_windowed(config_.get_bool("r_windowed", true));

        std::uint64_t previous = SDL_GetTicks64();
        while (state_.state() != gl::core::AppState::exit) {
            const std::uint64_t now = SDL_GetTicks64();
            std::uint64_t delta_ms = now - previous;
            previous = now;
            if (delta_ms > 250) delta_ms = 250;

            const float delta_seconds = static_cast<float>(delta_ms) / 1000.0f;
            SDL_Event event;
            while (SDL_PollEvent(&event) != 0) {
                handle_event(event);
            }
            if (state_.state() == gl::core::AppState::exit) break;

            server_.tick(delta_seconds);
            client_.on_snapshot(server_.snapshot());
            process_server_events();

            if (state_.state() == gl::core::AppState::level && level_ && !console_visible_) {
                int move_x = 0;
                int move_y = 0;
                const Uint8* keys = SDL_GetKeyboardState(nullptr);
                if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) move_x -= 1;
                if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) move_x += 1;
                if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) move_y -= 1;
                if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) move_y += 1;
                if (move_x != 0 || move_y != 0) {
                    level_->pan_input(move_x, move_y, delta_seconds);
                }
            }

            render(SDL_GetTicks64(), client_.snapshot().simulation_time_ms);
            SDL_RenderPresent(window_.renderer());
            SDL_Delay(1);
        }

        if (console_visible_) SDL_StopTextInput();
        return save_config() ? 0 : 2;
    }

  private:
    void load_font() {
        if (options_.no_font) {
            gl::log(gl::LogLevel::info, "font: --no-font forced ASCII bitmap text");
            return;
        }

        std::vector<fs::path> candidates;
        if (options_.font_set) candidates.push_back(options_.font);
        if (const std::optional<fs::path> system = gl::render::find_system_font()) {
            if (candidates.empty() || candidates.front() != *system) candidates.push_back(*system);
        }

        for (const fs::path& path : candidates) {
            gl::Result<gl::render::Font> loaded =
                gl::render::Font::load_from_file(window_.renderer(), path, kFontPixelHeight);
            if (!loaded) {
                gl::log(gl::LogLevel::warning, loaded.error().message);
                continue;
            }
            font_.emplace(std::move(loaded.value()));
            gl::log(gl::LogLevel::info, "font: loaded '" + path.string() + "'");
            break;
        }
        if (!font_) {
            gl::log(gl::LogLevel::warning, "font: no TrueType font available, using ASCII fallback");
        }
    }

    gl::Result<void> open_dialog(std::string_view virtual_path) {
        const auto report = [&](const gl::Error& error) {
            gl::log(gl::LogLevel::error,
                    "dialog '" + std::string(virtual_path) + "': " + error.message);
            return error;
        };

        auto script_data = vfs_.read(virtual_path);
        if (!script_data) return report(script_data.error());
        auto nodes = gl::assets::parse_cs(script_data.value());
        if (!nodes) return report(nodes.error());

        auto globals_data = vfs_.read(kDialogGlobals);
        if (!globals_data) return report(globals_data.error());
        auto globals = gl::script::parse_globals(globals_data.value());
        if (!globals) return report(globals.error());

        auto sdb_data = vfs_.read(kDialogSdb);
        if (!sdb_data) return report(sdb_data.error());
        auto sdb = gl::assets::Sdb::parse(sdb_data.value());
        if (!sdb) return report(sdb.error());

        auto context = std::make_unique<DialogContext>();
        context->path = std::string(virtual_path);
        context->sdb = std::move(sdb.value());
        context->vm =
            std::make_unique<gl::script::ScriptVm>(std::move(nodes.value()), context->host);
        context->vm->set_globals(globals.value());
        context->session = std::make_unique<gl::script::DialogSession>(*context->vm, context->sdb);

        const gl::Result<void> started = context->session->start();
        if (!started) {
            gl::log(gl::LogLevel::error,
                    "dialog '" + context->path + "': " + started.error().message);
            return started.error();
        }

        dialog_ = std::move(context);
        dialog_selected_ = 0;
        dialog_message_.clear();
        gl::log(gl::LogLevel::info,
                "dialog: '" + dialog_->path + "' answers=" +
                    std::to_string(dialog_->session->answers().size()));
        return {};
    }

    void close_dialog() {
        dialog_.reset();
        dialog_message_.clear();
        dialog_selected_ = 0;
    }

    bool dialog_waiting() const {
        return dialog_ && dialog_->session && dialog_->session->waiting();
    }

    float ui_line_height() const {
        if (font_) return font_->line_height();
        return std::max(gl::render::text_height(kScale), 1.0f);
    }

    float ui_text_width(std::string_view text) const {
        if (font_) return font_->measure(text).x;
        return gl::render::text_width(text, kScale);
    }

    void draw_ui_text(std::string_view text, float x, float y, gl::render::Color color) const {
        if (font_) {
            font_->draw_text(window_.renderer(), text, x, y, color);
        } else {
            gl::render::draw_text(window_.renderer(), text, x, y, kScale, color);
        }
    }

    std::vector<std::string> ui_wrap(std::string_view text, float max_width) const {
        if (font_) return font_->wrap(text, max_width);

        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start <= text.size()) {
            const std::size_t end = text.find('\n', start);
            const std::string_view paragraph =
                end == std::string_view::npos ? text.substr(start) : text.substr(start, end - start);
            std::string line;
            std::size_t index = 0;
            while (index < paragraph.size()) {
                while (index < paragraph.size() && paragraph[index] == ' ') ++index;
                const std::size_t word_start = index;
                while (index < paragraph.size() && paragraph[index] != ' ') ++index;
                if (word_start == index) continue;
                std::string candidate = line;
                if (!candidate.empty()) candidate.push_back(' ');
                candidate.append(paragraph.substr(word_start, index - word_start));
                if (!line.empty() && ui_text_width(candidate) > max_width) {
                    lines.push_back(line);
                    line.assign(paragraph.substr(word_start, index - word_start));
                } else {
                    line = std::move(candidate);
                }
            }
            lines.push_back(line);
            if (end == std::string_view::npos) break;
            start = end + 1;
        }
        return lines;
    }

    float dialog_panel_height() const {
        const float requested = static_cast<float>(viewport_height_) * 0.55f;
        return std::min(std::max(requested, 220.0f),
                        std::max(static_cast<float>(viewport_height_) - 40.0f, 220.0f));
    }

    float dialog_panel_y() const {
        return static_cast<float>(viewport_height_) - dialog_panel_height();
    }

    float dialog_text_width() const {
        return std::max(static_cast<float>(viewport_width_) - 2.0f * kDialogMargin, 40.0f);
    }

    bool dialog_has_bottom_message() const {
        return dialog_waiting() && !dialog_message_.empty();
    }

    float dialog_content_bottom() const {
        return static_cast<float>(viewport_height_) -
               (dialog_has_bottom_message() ? 68.0f : 40.0f);
    }

    float dialog_message_y() const {
        return static_cast<float>(viewport_height_) - 56.0f;
    }

    std::vector<std::string> dialog_npc_lines() const {
        if (!dialog_waiting()) return {};
        return ui_wrap(dialog_->session->npc_text(), dialog_text_width());
    }

    std::string dialog_answer_label(int index) const {
        if (!dialog_waiting()) return {};
        const std::span<const gl::script::DialogLine> answers = dialog_->session->answers();
        if (index < 0 || static_cast<std::size_t>(index) >= answers.size()) return {};
        return std::to_string(index + 1) + ". " +
               answers[static_cast<std::size_t>(index)].text;
    }

    float dialog_answers_top() const {
        const float line_height = ui_line_height();
        const float content_bottom = dialog_content_bottom();
        float y = dialog_panel_y() + kDialogMargin;
        if (dialog_waiting()) {
            const std::vector<std::string> lines = dialog_npc_lines();
            for (std::size_t index = 0; index < lines.size(); ++index) {
                if (y + line_height > content_bottom) break;
                y += line_height + 2.0f;
            }
        } else {
            y += line_height + 2.0f;
        }
        return y + 12.0f;
    }

    float dialog_answer_height(int index) const {
        const std::vector<std::string> lines =
            ui_wrap(dialog_answer_label(index), dialog_text_width() - 16.0f);
        const std::size_t count = std::max<std::size_t>(lines.size(), 1);
        return static_cast<float>(count) * ui_line_height() + kDialogAnswerGap;
    }

    float dialog_answer_top(int index) const {
        float y = dialog_answers_top();
        for (int i = 0; i < index; ++i) y += dialog_answer_height(i);
        return y;
    }

    SDL_FRect dialog_answer_rect(int index) const {
        const float top = dialog_answer_top(index);
        const float bottom =
            std::min(top + dialog_answer_height(index) - 2.0f, dialog_content_bottom());
        return SDL_FRect{kDialogMargin, top, dialog_text_width(),
                         std::max(bottom - top, 0.0f)};
    }

    void dialog_move(int delta) {
        if (!dialog_waiting()) return;
        const int count = static_cast<int>(dialog_->session->answers().size());
        if (count <= 0) return;
        dialog_selected_ = (dialog_selected_ + delta + count) % count;
    }

    void dialog_activate() {
        if (!dialog_waiting()) return;
        const std::span<const gl::script::DialogLine> answers = dialog_->session->answers();
        if (answers.empty() || dialog_selected_ < 0 ||
            static_cast<std::size_t>(dialog_selected_) >= answers.size()) {
            return;
        }
        const gl::Result<void> chosen =
            dialog_->session->choose(static_cast<std::size_t>(dialog_selected_));
        if (!chosen) {
            gl::log(gl::LogLevel::error, "dialog: " + chosen.error().message);
            dialog_message_ = "dialog error: " + chosen.error().message;
            return;
        }
        dialog_selected_ = 0;
        if (dialog_->session->waiting()) {
            dialog_message_.clear();
        } else {
            dialog_message_ = "Dialog ended. Press Esc.";
        }
    }

    void dialog_hover(int x, int y) {
        if (!dialog_waiting()) return;
        const int count = static_cast<int>(dialog_->session->answers().size());
        for (int index = 0; index < count; ++index) {
            const SDL_FRect rect = dialog_answer_rect(index);
            if (static_cast<float>(x) >= rect.x && static_cast<float>(x) < rect.x + rect.w &&
                static_cast<float>(y) >= rect.y && static_cast<float>(y) < rect.y + rect.h) {
                dialog_selected_ = index;
                return;
            }
        }
    }

    void dialog_click(int x, int y) {
        if (!dialog_waiting()) return;
        const int count = static_cast<int>(dialog_->session->answers().size());
        for (int index = 0; index < count; ++index) {
            const SDL_FRect rect = dialog_answer_rect(index);
            if (static_cast<float>(x) >= rect.x && static_cast<float>(x) < rect.x + rect.w &&
                static_cast<float>(y) >= rect.y && static_cast<float>(y) < rect.y + rect.h) {
                dialog_selected_ = index;
                dialog_activate();
                return;
            }
        }
    }

    void handle_dialog_key(const SDL_KeyboardEvent& key) {
        const SDL_Keycode symbol = key.keysym.sym;
        if (symbol == SDLK_ESCAPE) {
            if (dialog_launched_from_cli_) {
                request_exit();
            } else {
                close_dialog();
            }
        } else if (symbol == SDLK_UP) {
            dialog_move(-1);
        } else if (symbol == SDLK_DOWN) {
            dialog_move(1);
        } else if (symbol == SDLK_RETURN || symbol == SDLK_KP_ENTER) {
            dialog_activate();
        }
    }

    void register_commands() {
        registry_.add("help", "list commands or show details: help [command]",
                      [this](gl::core::Args args) -> gl::Result<std::string> {
                          if (args.empty()) return registry_.help();
                          if (args.size() != 1) {
                              return gl::Error{gl::ErrorCode::argument, "usage: help [command]"};
                          }
                          for (const gl::core::CommandInfo& command : registry_.commands()) {
                              if (command.name == args[0]) {
                                  return command.name + " - " + command.help;
                              }
                          }
                          return gl::Error{gl::ErrorCode::format,
                                           "unknown command: " + std::string(args[0])};
                      });

        registry_.add("exit", "quit the game", [this](gl::core::Args) -> gl::Result<std::string> {
            request_exit();
            return std::string{};
        });

        registry_.add("d_console", "toggle console or set it: d_console [0|1]",
                      [this](gl::core::Args args) -> gl::Result<std::string> {
                          if (args.size() > 1) {
                              return gl::Error{gl::ErrorCode::argument,
                                               "usage: d_console [0|1]"};
                          }
                          bool visible = !console_visible_;
                          if (args.size() == 1) {
                              if (args[0] == "0") {
                                  visible = false;
                              } else if (args[0] == "1") {
                                  visible = true;
                              } else {
                                  return gl::Error{gl::ErrorCode::argument,
                                                   "usage: d_console [0|1]"};
                              }
                          }
                          set_console_visible(visible);
                          config_.set_int("d_console", visible ? 1 : 0);
                          dirty_ = true;
                          save_config();
                          return "console = " + std::string(visible ? "1" : "0");
                      });

        registry_.add("gv_pause", "pause or resume animations: gv_pause [0|1]",
                      [this](gl::core::Args args) -> gl::Result<std::string> {
                          return route_server("gv_pause", args);
                      });

        registry_.add("gv_free_camera", "toggle free camera: gv_free_camera [0|1]",
                      [this](gl::core::Args args) -> gl::Result<std::string> {
                          const gl::Result<std::string> result =
                              route_server("gv_free_camera", args);
                          if (!result) return result;
                          return std::string("free_camera = ") +
                                 (server_.free_camera() ? "1" : "0");
                      });

        registry_.add("r_resolution", "set window resolution: r_resolution <width> <height>",
                      [this](gl::core::Args args) -> gl::Result<std::string> {
                          return route_server("r_resolution", args);
                      });

        registry_.add("r_windowed", "set windowed/fullscreen: r_windowed <0|1>",
                      [this](gl::core::Args args) -> gl::Result<std::string> {
                          return route_server("r_windowed", args);
                      });

        registry_.add("cl_username", "set player name: cl_username <name>",
                      [this](gl::core::Args args) -> gl::Result<std::string> {
                          return route_server("cl_username", args);
                      });

        registry_.add("saves", "list save slots",
                      [this](gl::core::Args args) -> gl::Result<std::string> {
                          if (!args.empty()) {
                              return gl::Error{gl::ErrorCode::argument, "usage: saves"};
                          }
                          return saves_text();
                      });

        registry_.add("game_save", "save the game (stub): game_save <slot>",
                      [](gl::core::Args args) -> gl::Result<std::string> {
                          if (args.size() != 1) {
                              return gl::Error{gl::ErrorCode::argument,
                                               "usage: game_save <slot>"};
                          }
                          return "game_save: saving is not implemented yet (slot " +
                                 std::string(args[0]) + ")";
                      });
    }

    gl::Result<std::string> route_server(std::string_view name, gl::core::Args args) {
        gl::core::Command command;
        command.name = std::string(name);
        command.args.reserve(args.size());
        for (const std::string_view arg : args) command.args.emplace_back(arg);
        server_.submit(std::move(command));
        server_.tick(0.0);
        client_.on_snapshot(server_.snapshot());
        process_server_events();
        return std::string{};
    }

    void apply_initial_server_config() {
        if (config_.get_bool("gv_pause", false)) server_.submit("gv_pause 1");
        if (config_.get_bool("gv_free_camera", false)) server_.submit("gv_free_camera 1");
        server_.tick(0.0);
        client_.on_snapshot(server_.snapshot());
        process_server_events();
    }

    void process_server_events() {
        for (const gl::core::Event& event : server_.drain_events()) {
            client_.on_event(event);
        }
        for (const gl::core::Event& event : client_.events()) {
            switch (event.type) {
                case gl::core::EventType::paused_changed:
                    if (config_.get_bool("gv_pause", false) != event.flag) {
                        config_.set_int("gv_pause", event.flag ? 1 : 0);
                        dirty_ = true;
                        save_config();
                    }
                    console_print(event.flag ? "paused = 1" : "paused = 0");
                    break;
                case gl::core::EventType::config_changed:
                    if (event.key == "r_resolution") {
                        const Resolution resolution =
                            parse_resolution(std::optional<std::string>(event.value));
                        if (!resolution.valid) {
                            const std::string message =
                                "invalid resolution: " + event.value;
                            console_print(message);
                            gl::log(gl::LogLevel::warning, message);
                            break;
                        }
                        config_.set(event.key, event.value);
                        dirty_ = true;
                        apply_resolution(resolution.width, resolution.height);
                    } else {
                        config_.set(event.key, event.value);
                        dirty_ = true;
                        if (event.key == "r_windowed") apply_windowed(event.flag);
                    }
                    save_config();
                    console_print(event.key + " = " + event.value);
                    break;
                case gl::core::EventType::log:
                    console_print(event.text);
                    gl::log(gl::LogLevel::warning, event.text);
                    break;
            }
        }
        client_.clear_events();
    }

    bool save_config() {
        if (!dirty_) return true;
        const gl::Result<void> result = config_.save(config_path_);
        if (!result) {
            gl::log(gl::LogLevel::error, result.error().message);
            console_print("cannot save config: " + result.error().message);
            return false;
        }
        dirty_ = false;
        return true;
    }

    void console_print(std::string_view text) {
        if (text.empty()) return;
        std::size_t start = 0;
        while (start < text.size()) {
            const std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos) {
                console_lines_.push(std::string(text.substr(start)));
                break;
            }
            console_lines_.push(std::string(text.substr(start, end - start)));
            start = end + 1;
        }
    }

    void execute_line(std::string_view line, bool add_history) {
        if (is_blank(line)) return;
        if (add_history) console_lines_.add_history(std::string(line));
        const gl::Result<std::string> result = registry_.execute(line);
        if (!result) {
            console_print("error: " + result.error().message);
            gl::log(gl::LogLevel::warning, result.error().message);
            return;
        }
        if (!result.value().empty()) {
            gl::log(gl::LogLevel::info, summarize_output(line, result.value()));
        }
        console_print(result.value());
    }

    bool load_level(std::string_view name) {
        auto screen = gl::app::LevelScreen::load(window_.renderer(), vfs_, name,
                                                 !options_.no_persons);
        if (!screen) {
            gl::log(gl::LogLevel::error, screen.error().message);
            console_print("cannot load level: " + screen.error().message);
            return false;
        }
        level_.emplace(std::move(screen.value()));
        level_name_ = std::string(name);
        update_viewport();
        return true;
    }

    void request_exit() {
        state_.transition(gl::core::AppState::exit);
    }

    void set_console_visible(bool visible) {
        if (console_visible_ == visible) return;
        console_visible_ = visible;
        if (visible) {
            console_lines_.clear_history_navigation();
            SDL_StartTextInput();
        } else {
            input_.clear();
            SDL_StopTextInput();
        }
    }

    void apply_windowed(bool windowed) {
        if (screenshot_mode_) return;
        const Uint32 flags = windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (SDL_SetWindowFullscreen(window_.window(), flags) != 0) {
            gl::log(gl::LogLevel::warning,
                    std::string("cannot change window mode: ") + SDL_GetError());
        }
    }

    void apply_resolution(int width, int height) {
        if (screenshot_mode_) return;
        SDL_SetWindowSize(window_.window(), width, height);
        update_viewport();
    }

    void update_viewport() {
        int width = 0;
        int height = 0;
        if (SDL_GetRendererOutputSize(window_.renderer(), &width, &height) != 0 || width <= 0 ||
            height <= 0) {
            return;
        }
        viewport_width_ = width;
        viewport_height_ = height;
        if (level_) level_->set_viewport(width, height);
    }

    void handle_event(const SDL_Event& event) {
        switch (event.type) {
            case SDL_QUIT:
                request_exit();
                return;
            case SDL_KEYDOWN:
                handle_keydown(event.key);
                return;
            case SDL_TEXTINPUT:
                if (console_visible_) {
                    const std::string_view text = event.text.text;
                    if (text == "`" || text == "~") return;
                    input_ += text;
                }
                return;
            case SDL_MOUSEMOTION:
                if (console_visible_) return;
                if (dialog_) {
                    dialog_hover(event.motion.x, event.motion.y);
                } else if (save_slots_) {
                    save_hover(event.motion.x, event.motion.y);
                } else if (state_.state() == gl::core::AppState::menu) {
                    menu_hover(event.motion.x, event.motion.y);
                } else if (state_.state() == gl::core::AppState::level && dragging_ &&
                           level_) {
                    const float zoom = level_->camera().zoom;
                    level_->pan(-static_cast<float>(event.motion.xrel) / zoom,
                                -static_cast<float>(event.motion.yrel) / zoom);
                }
                return;
            case SDL_MOUSEBUTTONDOWN:
                if (console_visible_ || event.button.button != SDL_BUTTON_LEFT) return;
                if (dialog_) {
                    dialog_click(event.button.x, event.button.y);
                } else if (save_slots_) {
                    save_click(event.button.x, event.button.y);
                } else if (state_.state() == gl::core::AppState::menu) {
                    menu_click(event.button.x, event.button.y);
                } else if (state_.state() == gl::core::AppState::level) {
                    dragging_ = true;
                }
                return;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) dragging_ = false;
                return;
            case SDL_MOUSEWHEEL:
                if (console_visible_ || state_.state() != gl::core::AppState::level ||
                    !level_ || event.wheel.y == 0) {
                    return;
                }
                {
                    int mouse_x = 0;
                    int mouse_y = 0;
                    SDL_GetMouseState(&mouse_x, &mouse_y);
                    const float factor = event.wheel.y > 0 ? kZoomStep : 1.0f / kZoomStep;
                    level_->zoom_at(factor, static_cast<float>(mouse_x),
                                    static_cast<float>(mouse_y));
                }
                return;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    update_viewport();
                } else if (event.window.event == SDL_WINDOWEVENT_LEAVE ||
                           event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    dragging_ = false;
                }
                return;
            default:
                return;
        }
    }

    void handle_keydown(const SDL_KeyboardEvent& key) {
        const SDL_Keycode symbol = key.keysym.sym;
        if (console_visible_) {
            handle_console_key(key);
            return;
        }
        if (symbol == SDLK_BACKQUOTE) {
            set_console_visible(true);
            return;
        }
        if (key.repeat != 0 && symbol != SDLK_HOME) return;

        if (dialog_) {
            handle_dialog_key(key);
            return;
        }

        if (save_slots_) {
            if (symbol == SDLK_UP) {
                save_move(-1);
            } else if (symbol == SDLK_DOWN) {
                save_move(1);
            } else if (symbol == SDLK_RETURN || symbol == SDLK_KP_ENTER) {
                save_activate();
            } else if (symbol == SDLK_ESCAPE) {
                save_slots_ = false;
                save_message_.clear();
            }
            return;
        }

        if (state_.state() == gl::core::AppState::menu) {
            if (symbol == SDLK_UP) {
                menu_move(-1);
            } else if (symbol == SDLK_DOWN) {
                menu_move(1);
            } else if (symbol == SDLK_RETURN || symbol == SDLK_KP_ENTER) {
                menu_activate(menu_selected_);
            } else if (symbol == SDLK_ESCAPE) {
                request_exit();
            }
            return;
        }

        if (state_.state() == gl::core::AppState::level && level_) {
            if (symbol == SDLK_ESCAPE) {
                dragging_ = false;
                level_.reset();
                state_.transition(gl::core::AppState::menu);
            } else if (symbol == SDLK_HOME) {
                level_->reset_camera();
            } else if (symbol == SDLK_F1) {
                debug_overlay_ = !debug_overlay_;
            }
        }
    }

    void handle_console_key(const SDL_KeyboardEvent& key) {
        const SDL_Keycode symbol = key.keysym.sym;
        if (symbol == SDLK_BACKQUOTE || symbol == SDLK_ESCAPE) {
            set_console_visible(false);
        } else if (symbol == SDLK_BACKSPACE) {
            erase_last_utf8(input_);
        } else if (symbol == SDLK_RETURN || symbol == SDLK_KP_ENTER) {
            const std::string line = input_;
            input_.clear();
            console_lines_.clear_history_navigation();
            execute_line(line, true);
        } else if (symbol == SDLK_UP) {
            input_ = console_lines_.history_prev();
        } else if (symbol == SDLK_DOWN) {
            input_ = console_lines_.history_next();
        } else if (symbol == SDLK_TAB) {
            const std::optional<std::string> completion = registry_.complete(input_);
            if (completion) input_ = *completion + " ";
        }
    }

    int menu_item_count() const {
        return test_dialog_available_ ? kBaseMenuItems + 1 : kBaseMenuItems;
    }

    const char* menu_label(int index) const {
        if (index == 0) return "New Game";
        if (index == 1) return "Continue";
        if (test_dialog_available_ && index == 2) return "Test Dialog";
        return "Exit";
    }

    bool menu_item_enabled(int index) const {
        return index != 1 || saves_.has_valid_slots();
    }

    float menu_item_height() const { return gl::render::text_height(kScale) + 18.0f; }

    float menu_first_item_y() const {
        return static_cast<float>(viewport_height_) * 0.52f;
    }

    SDL_FRect menu_item_rect(int index) const {
        const float item_height = menu_item_height();
        const float width = static_cast<float>(viewport_width_) * 0.36f;
        const float x = (static_cast<float>(viewport_width_) - width) * 0.5f;
        const float y = menu_first_item_y() + static_cast<float>(index) * item_height;
        return SDL_FRect{x, y, width, item_height - 4.0f};
    }

    void menu_move(int delta) {
        const int count = menu_item_count();
        int index = menu_selected_;
        for (int step = 0; step < count; ++step) {
            index = (index + delta + count) % count;
            if (menu_item_enabled(index)) {
                menu_selected_ = index;
                return;
            }
        }
    }

    void menu_hover(int x, int y) {
        const int count = menu_item_count();
        for (int index = 0; index < count; ++index) {
            if (!menu_item_enabled(index)) continue;
            const SDL_FRect rect = menu_item_rect(index);
            if (static_cast<float>(x) >= rect.x && static_cast<float>(x) < rect.x + rect.w &&
                static_cast<float>(y) >= rect.y && static_cast<float>(y) < rect.y + rect.h) {
                menu_selected_ = index;
                return;
            }
        }
    }

    void menu_click(int x, int y) {
        const int count = menu_item_count();
        for (int index = 0; index < count; ++index) {
            if (!menu_item_enabled(index)) continue;
            const SDL_FRect rect = menu_item_rect(index);
            if (static_cast<float>(x) >= rect.x && static_cast<float>(x) < rect.x + rect.w &&
                static_cast<float>(y) >= rect.y && static_cast<float>(y) < rect.y + rect.h) {
                menu_selected_ = index;
                menu_activate(index);
                return;
            }
        }
    }

    void menu_activate(int index) {
        if (index == 0) {
            if (!load_level(kNewGameLevel)) return;
            if (state_.transition(gl::core::AppState::level)) {
                dragging_ = false;
                debug_overlay_ = false;
            }
        } else if (index == 1) {
            if (!saves_.has_valid_slots()) return;
            save_slots_ = true;
            save_selected_ = 0;
            save_scroll_ = 0;
            save_message_.clear();
        } else if (test_dialog_available_ && index == 2) {
            const gl::Result<void> opened = open_dialog(kTestDialogScript);
            if (!opened) {
                gl::log(gl::LogLevel::error, opened.error().message);
                console_print("cannot open dialog: " + opened.error().message);
            }
        } else {
            request_exit();
        }
    }

    void rebuild_display_saves() {
        display_saves_.clear();
        for (const gl::core::SaveSlotInfo& slot : saves_.slots()) {
            if (is_displayable(slot)) display_saves_.push_back(&slot);
        }
    }

    const std::vector<const gl::core::SaveSlotInfo*>& visible_saves() const {
        return display_saves_;
    }

    std::string saves_text() const {
        std::string text;
        for (const gl::core::SaveSlotInfo* slot : display_saves_) {
            if (!text.empty()) text.push_back('\n');
            text += format_save_row(*slot);
        }
        if (text.empty()) return "no saves";
        return text;
    }

    float save_rows_top() const {
        return std::max(110.0f, static_cast<float>(viewport_height_) * 0.22f);
    }

    float save_rows_bottom() const {
        return static_cast<float>(viewport_height_) - 80.0f;
    }

    float save_row_height() const {
        if (font_) return std::max(font_->line_height() + 3.0f, kSaveRowHeight);
        return kSaveRowHeight;
    }

    int save_visible_row_count() const {
        const float row_height = save_row_height();
        const float available = save_rows_bottom() - save_rows_top();
        if (available < row_height) return 1;
        return std::max(1, static_cast<int>(available / row_height));
    }

    void save_ensure_visible() {
        const int count = static_cast<int>(display_saves_.size());
        if (count <= 0) {
            save_selected_ = 0;
            save_scroll_ = 0;
            return;
        }
        save_selected_ = std::clamp(save_selected_, 0, count - 1);
        const int visible = save_visible_row_count();
        const int max_scroll = std::max(0, count - visible);
        if (save_selected_ < save_scroll_) save_scroll_ = save_selected_;
        if (save_selected_ >= save_scroll_ + visible) {
            save_scroll_ = save_selected_ - visible + 1;
        }
        save_scroll_ = std::clamp(save_scroll_, 0, max_scroll);
    }

    float save_row_y(int index) const {
        const int row = index - save_scroll_;
        return save_rows_top() + static_cast<float>(row) * save_row_height();
    }

    SDL_FRect save_row_rect(int index) const {
        const float width = static_cast<float>(viewport_width_) * 0.7f;
        const float x = (static_cast<float>(viewport_width_) - width) * 0.5f;
        return SDL_FRect{x, save_row_y(index), width, save_row_height() - 2.0f};
    }

    void save_move(int delta) {
        const int count = static_cast<int>(display_saves_.size());
        if (count <= 0) return;
        save_selected_ = (save_selected_ + delta + count) % count;
        save_ensure_visible();
    }

    void save_hover(int x, int y) {
        const int count = static_cast<int>(display_saves_.size());
        const int end = std::min(count, save_scroll_ + save_visible_row_count());
        for (int index = save_scroll_; index < end; ++index) {
            const SDL_FRect rect = save_row_rect(index);
            if (static_cast<float>(x) >= rect.x && static_cast<float>(x) < rect.x + rect.w &&
                static_cast<float>(y) >= rect.y && static_cast<float>(y) < rect.y + rect.h) {
                save_selected_ = index;
                return;
            }
        }
    }

    void save_click(int x, int y) {
        const int count = static_cast<int>(display_saves_.size());
        const int end = std::min(count, save_scroll_ + save_visible_row_count());
        for (int index = save_scroll_; index < end; ++index) {
            const SDL_FRect rect = save_row_rect(index);
            if (static_cast<float>(x) >= rect.x && static_cast<float>(x) < rect.x + rect.w &&
                static_cast<float>(y) >= rect.y && static_cast<float>(y) < rect.y + rect.h) {
                save_selected_ = index;
                save_activate();
                return;
            }
        }
    }

    void save_activate() {
        if (display_saves_.empty()) return;
        save_message_ = "world deserialization not implemented (P2c)";
    }

    void render(std::uint64_t now_ms, std::uint64_t animation_ms) {
        update_viewport();
        if (dialog_ && dialog_->session) {
            render_dialog();
        } else if (state_.state() == gl::core::AppState::level && level_) {
            gl::app::LevelScreenOptions options;
            options.debug_overlay = debug_overlay_;
            options.walkable_overlay = options_.walkable_overlay;
            options.draw_persons = !options_.no_persons;
            level_->render(window_.renderer(), options, animation_ms);
            if (client_.snapshot().paused) {
                const std::string_view label = "PAUSED";
                const float width = gl::render::text_width(label, kTitleScale);
                gl::render::draw_text(window_.renderer(), label,
                                      (static_cast<float>(viewport_width_) - width) * 0.5f,
                                      24.0f, kTitleScale,
                                      gl::render::Color{255, 210, 90, 255});
            }
        } else if (save_slots_) {
            render_save_slots();
        } else {
            render_menu();
        }

        if (console_visible_) render_console(now_ms);
    }

    void render_dialog() {
        SDL_Renderer* renderer = window_.renderer();
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 12, 14, 20, 255);
        SDL_RenderClear(renderer);

        const float panel_y = dialog_panel_y();
        const float panel_height = dialog_panel_height();
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 5, 7, 11, 235);
        const SDL_FRect panel{0.0f, panel_y, static_cast<float>(viewport_width_), panel_height};
        SDL_RenderFillRectF(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 232, 201, 112, 255);
        SDL_RenderDrawLine(renderer, 0, static_cast<int>(panel_y), viewport_width_,
                           static_cast<int>(panel_y));

        const gl::render::Color text_color{232, 232, 238, 255};
        const gl::render::Color answer_color{210, 214, 222, 255};
        const gl::render::Color selected_color{255, 232, 160, 255};
        const float line_height = ui_line_height();
        const float content_bottom = dialog_content_bottom();

        float y = panel_y + kDialogMargin;
        if (dialog_waiting()) {
            const std::vector<std::string> lines = dialog_npc_lines();
            for (const std::string& line : lines) {
                if (y + line_height > content_bottom) break;
                draw_ui_text(line, kDialogMargin, y, text_color);
                y += line_height + 2.0f;
            }
        } else {
            const std::string message =
                dialog_message_.empty() ? "Dialog ended. Press Esc." : dialog_message_;
            draw_ui_text(message, kDialogMargin, y, selected_color);
            y += line_height + 2.0f;
        }

        if (dialog_waiting()) {
            const std::span<const gl::script::DialogLine> answers = dialog_->session->answers();
            if (answers.empty()) {
                const float message_y = dialog_answers_top();
                if (message_y + line_height <= content_bottom) {
                    draw_ui_text("No answers. Press Esc.", kDialogMargin, message_y,
                                 selected_color);
                }
            } else {
                const int count = static_cast<int>(answers.size());
                for (int index = 0; index < count; ++index) {
                    const SDL_FRect rect = dialog_answer_rect(index);
                    if (rect.y >= content_bottom) break;
                    const bool selected = dialog_selected_ == index;
                    if (selected) {
                        SDL_SetRenderDrawColor(renderer, 60, 52, 24, 210);
                        SDL_RenderFillRectF(renderer, &rect);
                        SDL_SetRenderDrawColor(renderer, 232, 201, 112, 255);
                        SDL_RenderDrawRectF(renderer, &rect);
                    }

                    const std::vector<std::string> lines =
                        ui_wrap(dialog_answer_label(index), rect.w - 16.0f);
                    float text_y = rect.y + 2.0f;
                    for (const std::string& line : lines) {
                        if (text_y + line_height > content_bottom) break;
                        draw_ui_text(line, rect.x + 8.0f, text_y,
                                     selected ? selected_color : answer_color);
                        text_y += line_height + 1.0f;
                    }
                }
            }
        }

        const float hint_y = static_cast<float>(viewport_height_) - 28.0f;
        if (dialog_has_bottom_message()) {
            draw_ui_text(dialog_message_, kDialogMargin, dialog_message_y(),
                         gl::render::Color{255, 170, 120, 255});
        }
        const bool has_answers =
            dialog_waiting() && !dialog_->session->answers().empty();
        const std::string_view hint = has_answers
                                          ? "Up/Down or mouse to select, Enter to choose, Esc to close"
                                          : "Esc to close";
        draw_ui_text(hint, kDialogMargin, hint_y, gl::render::Color{110, 112, 122, 255});
    }

    void render_save_slots() {
        SDL_Renderer* renderer = window_.renderer();
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 12, 14, 20, 255);
        SDL_RenderClear(renderer);

        save_ensure_visible();

        const std::string_view title = "SAVE SLOTS";
        const float title_width = gl::render::text_width(title, kTitleScale);
        const float title_y =
            std::max(16.0f, static_cast<float>(viewport_height_) * 0.1f);
        gl::render::draw_text(renderer, title,
                              (static_cast<float>(viewport_width_) - title_width) * 0.5f, title_y,
                              kTitleScale, gl::render::Color{232, 201, 112, 255});

        const std::vector<const gl::core::SaveSlotInfo*>& slots = visible_saves();
        const int count = static_cast<int>(slots.size());
        const int end = std::min(count, save_scroll_ + save_visible_row_count());
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        const SDL_Rect clip{0, static_cast<int>(save_rows_top()) - 2, viewport_width_,
                            static_cast<int>(save_rows_bottom() - save_rows_top()) + 4};
        SDL_RenderSetClipRect(renderer, &clip);
        for (int index = save_scroll_; index < end; ++index) {
            const SDL_FRect rect = save_row_rect(index);
            const bool selected = save_selected_ == index;
            if (selected) {
                SDL_SetRenderDrawColor(renderer, 60, 52, 24, 210);
                SDL_RenderFillRectF(renderer, &rect);
                SDL_SetRenderDrawColor(renderer, 232, 201, 112, 255);
                SDL_RenderDrawRectF(renderer, &rect);
            }
            const gl::render::Color color =
                selected ? gl::render::Color{255, 232, 160, 255}
                         : gl::render::Color{220, 220, 220, 255};
            draw_ui_text(format_save_row(*slots[static_cast<std::size_t>(index)]), rect.x + 12.0f,
                         rect.y + std::max((rect.h - ui_line_height()) * 0.5f, 0.0f), color);
        }
        SDL_RenderSetClipRect(renderer, nullptr);

        if (slots.empty()) {
            const std::string_view empty = "no saves";
            const float width = gl::render::text_width(empty, kScale);
            gl::render::draw_text(renderer, empty,
                                  (static_cast<float>(viewport_width_) - width) * 0.5f,
                                  static_cast<float>(viewport_height_) * 0.4f, kScale,
                                  gl::render::Color{160, 160, 168, 255});
        }

        if (!save_message_.empty()) {
            const float width = gl::render::text_width(save_message_, 1.0f);
            gl::render::draw_text(renderer, save_message_,
                                  (static_cast<float>(viewport_width_) - width) * 0.5f,
                                  static_cast<float>(viewport_height_) - 64.0f, 1.0f,
                                  gl::render::Color{255, 210, 90, 255});
        }

        const std::string_view hint = "Arrows / mouse to select, Enter to load, Esc to go back";
        const float hint_width = gl::render::text_width(hint, 1.0f);
        gl::render::draw_text(renderer, hint,
                              (static_cast<float>(viewport_width_) - hint_width) * 0.5f,
                              static_cast<float>(viewport_height_) - 30.0f, 1.0f,
                              gl::render::Color{100, 100, 110, 255});
    }

    void render_menu() {
        SDL_Renderer* renderer = window_.renderer();
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 12, 14, 20, 255);
        SDL_RenderClear(renderer);

        const std::string_view title = "GOLDENLAND 2";
        const float title_width = gl::render::text_width(title, kTitleScale);
        gl::render::draw_text(renderer, title,
                              (static_cast<float>(viewport_width_) - title_width) * 0.5f,
                              static_cast<float>(viewport_height_) * 0.3f, kTitleScale,
                              gl::render::Color{232, 201, 112, 255});

        const std::string_view subtitle = "Goldenland 2";
        const float subtitle_width = gl::render::text_width(subtitle, 1.0f);
        gl::render::draw_text(renderer, subtitle,
                              (static_cast<float>(viewport_width_) - subtitle_width) * 0.5f,
                              static_cast<float>(viewport_height_) * 0.3f + 44.0f, 1.0f,
                              gl::render::Color{130, 130, 140, 255});

        const int count = menu_item_count();
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (int index = 0; index < count; ++index) {
            const SDL_FRect rect = menu_item_rect(index);
            const bool enabled = menu_item_enabled(index);
            const bool selected = menu_selected_ == index && enabled;

            if (selected) {
                SDL_SetRenderDrawColor(renderer, 60, 52, 24, 210);
                SDL_RenderFillRectF(renderer, &rect);
                SDL_SetRenderDrawColor(renderer, 232, 201, 112, 255);
                SDL_RenderDrawRectF(renderer, &rect);
            }

            const gl::render::Color color = !enabled ? gl::render::Color{104, 104, 108, 255}
                                          : selected ? gl::render::Color{255, 232, 160, 255}
                                                     : gl::render::Color{220, 220, 220, 255};
            const char* label = menu_label(index);
            const float width = gl::render::text_width(label, kScale);
            const float text_x =
                rect.x + (rect.w - width) * 0.5f;
            const float text_y = rect.y + (rect.h - gl::render::text_height(kScale)) * 0.5f;
            gl::render::draw_text(renderer, label, text_x, text_y, kScale, color);
        }

        SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
        SDL_RenderDrawLine(renderer, static_cast<int>(static_cast<float>(viewport_width_) * 0.3f),
                           static_cast<int>(menu_first_item_y() - 18.0f),
                           static_cast<int>(static_cast<float>(viewport_width_) * 0.7f),
                           static_cast<int>(menu_first_item_y() - 18.0f));

        const std::string_view hint = "Arrows / mouse to select, Enter to activate, Esc to quit";
        const float hint_width = gl::render::text_width(hint, 1.0f);
        gl::render::draw_text(renderer, hint,
                              (static_cast<float>(viewport_width_) - hint_width) * 0.5f,
                              static_cast<float>(viewport_height_) - 30.0f, 1.0f,
                              gl::render::Color{100, 100, 110, 255});
    }

    void render_console(std::uint64_t now_ms) {
        SDL_Renderer* renderer = window_.renderer();
        const float line_height = gl::render::text_height(kScale) + 3.0f;
        const float padding = 8.0f;
        const float panel_height =
            line_height * static_cast<float>(kConsoleOutputLines + 1) + padding * 2.0f;
        const float panel_y = static_cast<float>(viewport_height_) - panel_height;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
        const SDL_FRect panel{0.0f, panel_y, static_cast<float>(viewport_width_), panel_height};
        SDL_RenderFillRectF(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 90, 120, 90, 255);
        SDL_RenderDrawLine(renderer, 0, static_cast<int>(panel_y), viewport_width_,
                           static_cast<int>(panel_y));

        const std::deque<std::string>& lines = console_lines_.lines();
        const std::size_t visible = std::min(lines.size(), kConsoleOutputLines);
        const std::size_t first = lines.size() - visible;
        float y = panel_y + padding;
        for (std::size_t index = first; index < lines.size(); ++index) {
            gl::render::draw_text(renderer, lines[index], padding, y, kScale,
                                  gl::render::Color{205, 210, 205, 255});
            y += line_height;
        }

        const float prompt_y = panel_y + padding + line_height *
                                                       static_cast<float>(kConsoleOutputLines);
        gl::render::draw_text(renderer, "> ", padding, prompt_y, kScale,
                              gl::render::Color{140, 220, 140, 255});
        gl::render::draw_text(renderer, input_, padding + gl::render::text_width("> ", kScale),
                              prompt_y, kScale, gl::render::Color{235, 235, 235, 255});

        if ((now_ms / 500) % 2 == 0) {
            const float cursor_x =
                padding + gl::render::text_width("> ", kScale) +
                gl::render::text_width(input_, kScale) + 1.0f;
            const SDL_FRect cursor{cursor_x, prompt_y,
                                   std::max(2.0f, 2.0f * kScale), line_height - 3.0f};
            SDL_SetRenderDrawColor(renderer, 200, 220, 200, 220);
            SDL_RenderFillRectF(renderer, &cursor);
        }
    }

    Options options_;
    gl::core::Config config_;
    fs::path config_path_;
    gl::assets::GameVfs& vfs_;
    gl::platform::Window& window_;
    gl::core::StateMachine state_;
    gl::core::SaveManager saves_;
    std::vector<const gl::core::SaveSlotInfo*> display_saves_;
    gl::core::ServerCore server_;
    gl::core::ClientCore client_;
    gl::core::CommandRegistry registry_;
    gl::core::ConsoleBuffer console_lines_;
    std::optional<gl::app::LevelScreen> level_;
    std::string level_name_;
    std::string input_;
    bool console_visible_ = false;
    bool screenshot_mode_ = false;
    bool dirty_ = false;
    bool dragging_ = false;
    bool debug_overlay_ = false;
    int viewport_width_ = 0;
    int viewport_height_ = 0;
    int menu_selected_ = 0;
    int save_selected_ = 0;
    int save_scroll_ = 0;
    bool save_slots_ = false;
    std::string save_message_;
    std::optional<gl::render::Font> font_;
    std::unique_ptr<DialogContext> dialog_;
    bool test_dialog_available_ = false;
    bool dialog_launched_from_cli_ = false;
    int dialog_selected_ = 0;
    std::string dialog_message_;
};

} // namespace

int main(int argc, char** argv) {
    Options options;
    try {
        const ParseStatus status = parse_args(argc, argv, options);
        if (status == ParseStatus::help) return 0;
        if (status == ParseStatus::error) return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glgame: %s\n", e.what());
        return 1;
    }

    try {
        std::error_code ec;
        if (!fs::is_directory(options.game_dir, ec)) {
            std::fprintf(stderr, "glgame: game directory not found: %s\n",
                         options.game_dir.string().c_str());
            return 2;
        }

        if (options.list_saves) {
            auto saves = gl::core::SaveManager::scan(options.game_dir / "Saves");
            if (!saves) {
                std::fprintf(stderr, "glgame: %s\n", saves.error().message.c_str());
                return exit_code_for(saves.error());
            }
            std::string output;
            for (const gl::core::SaveSlotInfo& slot : saves.value().slots()) {
                if (!is_displayable(slot)) continue;
                output += format_save_list_line(slot);
                output.push_back('\n');
            }
            std::fputs(output.c_str(), stdout);
            return 0;
        }

        const fs::path config_path =
            options.config_path.empty() ? options.game_dir / "GoldenLand.ini"
                                        : options.config_path;
        auto config = gl::core::Config::load(config_path);
        if (!config) {
            std::fprintf(stderr, "glgame: %s\n", config.error().message.c_str());
            return exit_code_for(config.error());
        }

        const Resolution resolution = parse_resolution(config.value().get("r_resolution"));
        if (resolution.valid) {
            if (!options.width_set) options.width = resolution.width;
            if (!options.height_set) options.height = resolution.height;
        }

        auto vfs = gl::assets::GameVfs::mount(options.game_dir);
        if (!vfs) {
            std::fprintf(stderr, "glgame: cannot mount game directory: %s\n",
                         vfs.error().message.c_str());
            return exit_code_for(vfs.error());
        }

        const fs::path screenshot = options.screenshot;

        gl::platform::SdlContext context;
        gl::platform::Window window("Goldenland 2", options.width, options.height);

        Game game(std::move(options), std::move(config.value()), config_path, vfs.value(),
                  window);
        const int boot = game.boot();
        if (boot != 0) return boot;

        if (!screenshot.empty()) return game.screenshot(screenshot);
        return game.run_interactive();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glgame: %s\n", e.what());
        return 2;
    }
}
