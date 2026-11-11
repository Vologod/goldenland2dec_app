#include <gl/app/click_actions.hpp>
#include <gl/app/dialog_overlay.hpp>
#include <gl/app/input_gate.hpp>
#include <gl/app/inventory_overlay.hpp>
#include <gl/app/level_screen.hpp>
#include <gl/app/pending_talk.hpp>
#include <gl/app/trade_overlay.hpp>
#include <gl/assets/cs.hpp>
#include <gl/assets/hero_equipment.hpp>
#include <gl/assets/hero_stats.hpp>
#include <gl/assets/herowear.hpp>
#include <gl/assets/item_catalog.hpp>
#include <gl/assets/item_name_index.hpp>
#include <gl/assets/log.hpp>
#include <gl/assets/sdb.hpp>
#include <gl/assets/specials.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/core/command.hpp>
#include <gl/core/config.hpp>
#include <gl/core/console.hpp>
#include <gl/core/save_manager.hpp>
#include <gl/core/server.hpp>
#include <gl/core/state.hpp>
#include <gl/game/dialog_path.hpp>
#include <gl/game/game_script_host.hpp>
#include <gl/game/hero_outfit_resolver.hpp>
#include <gl/game/inventory_runtime.hpp>
#include <gl/game/npc_loot.hpp>
#include <gl/game/person_runtime.hpp>
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
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

enum class TradeHookKind { buy, sell, exchange };

struct TradeHook {
    TradeHookKind kind = TradeHookKind::buy;
    std::string tech;
};

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
    bool no_specials = false;
    bool hero_outfit_none = false;
    std::optional<int> equip_save;
    bool equip_save_pending = false;
    bool inventory = false;
    std::optional<int> inventory_select;
    std::string trade_npc;
    bool has_trade_npc = false;
    std::vector<TradeHook> trade_hooks;
    std::optional<int> dialog_answer;
    std::optional<int> camera_x;
    std::optional<int> camera_y;
    bool walkable_overlay = false;
    std::uint64_t sim_ms = 0;
    std::optional<std::uint64_t> seed;
    std::string dialog;
    bool has_dialog = false;
    int click_npc = 0;
    bool has_click_npc = false;
    int click_world_x = 0;
    int click_world_y = 0;
    bool has_click_world = false;
    std::vector<std::string> dump_npcs;
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
        "  --no-specials        Disable NPC loot specials (deterministic no-specials mode)\n"
        "  --dump-npc <name>    Headless direct-level test: log generated main/trade items for <name>\n"
        "  --equip-save <slot>  Apply hero equipment from save slot <slot>\n"
        "  --equip-save-pending Apply --equip-save on the next level load only\n"
        "  --inventory          Open the inventory overlay for the selected save\n"
        "  --inventory-select <n> Select bottom-strip slot <n> when the inventory opens\n"
        "  --trade-npc <name>   Open the trade overlay for person <name> after loading\n"
        "  --trade-buy <tech>   Headless: stage the whole NPC stack for item <tech>\n"
        "  --trade-sell <tech>  Headless: stage the whole Hero stack for item <tech>\n"
        "  --trade-exchange     Headless: run Exchange for the staged trade rows\n"
        "  --dialog-answer <n>  Headless: choose dialog answer <n> when it opens\n"
        "  --camera-x <px>      World X of the left viewport edge (screenshot mode)\n"
        "  --camera-y <px>      World Y of the top viewport edge (screenshot mode)\n"
        "  --walkable-overlay   Draw blocked tiles in red on the level\n"
        "  --sim-ms <ms>        Simulation time for screenshots (ignored interactively)\n"
        "  --seed <n>           Deterministic RNG seed (default: clock-based)\n"
        "  --click-npc <index>  Headless test: click the person instance by index\n"
        "  --click-world <x> <y> Headless test: click the walk tile at x y\n"
        "  --list-saves         Print save slots as tab-separated lines and exit\n"
        "  --screenshot <png>   Render the current state, save PNG and exit\n"
        "  --exec \"<command>\"   Run a console command after boot (repeatable)\n"
        "  -h, --help           Show this help and exit\n"
        "\n"
        "Console commands include: help, exit, saves, inventory.\n"
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

bool parse_seed(std::string_view text, std::uint64_t& value) {
    if (text.empty()) return false;
    std::uint64_t parsed = 0;
    const char* const end = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(text.data(), end, parsed);
    if (error != std::errc{} || ptr != end) return false;
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
        } else if (arg == "--no-specials") {
            options.no_specials = true;
        } else if (arg == "--dump-npc") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value.empty()) {
                std::fputs("glgame: --dump-npc must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.dump_npcs.emplace_back(value);
        } else if (arg == "--hero-outfit") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value != "none") {
                std::fprintf(stderr, "glgame: --hero-outfit supports only 'none'\n");
                return ParseStatus::error;
            }
            options.hero_outfit_none = true;
        } else if (arg == "--equip-save") {
            if (!next_value(arg, value)) return ParseStatus::error;
            int slot = 0;
            if (!parse_int(value, slot) || slot < 0) {
                std::fprintf(stderr, "glgame: invalid --equip-save: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.equip_save = slot;
        } else if (arg == "--equip-save-pending") {
            options.equip_save_pending = true;
        } else if (arg == "--inventory") {
            options.inventory = true;
        } else if (arg == "--inventory-select") {
            if (!next_value(arg, value)) return ParseStatus::error;
            int slot = 0;
            if (!parse_int(value, slot) || slot < 0) {
                std::fprintf(stderr, "glgame: invalid --inventory-select: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.inventory_select = slot;
        } else if (arg == "--camera-x") {
            if (!next_value(arg, value)) return ParseStatus::error;
            int camera = 0;
            if (!parse_int(value, camera) || camera < 0 || camera > 1'000'000) {
                std::fprintf(stderr, "glgame: invalid --camera-x: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.camera_x = camera;
        } else if (arg == "--camera-y") {
            if (!next_value(arg, value)) return ParseStatus::error;
            int camera = 0;
            if (!parse_int(value, camera) || camera < 0 || camera > 1'000'000) {
                std::fprintf(stderr, "glgame: invalid --camera-y: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.camera_y = camera;
        } else if (arg == "--walkable-overlay") {
            options.walkable_overlay = true;
        } else if (arg == "--sim-ms") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (!parse_milliseconds(value, options.sim_ms)) {
                std::fprintf(stderr, "glgame: invalid --sim-ms: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
        } else if (arg == "--seed") {
            if (!next_value(arg, value)) return ParseStatus::error;
            std::uint64_t seed = 0;
            if (!parse_seed(value, seed)) {
                std::fprintf(stderr, "glgame: invalid --seed: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.seed = seed;
        } else if (arg == "--click-npc") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (!parse_int(value, options.click_npc) || options.click_npc < 0) {
                std::fprintf(stderr, "glgame: invalid --click-npc: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.has_click_npc = true;
        } else if (arg == "--click-world") {
            if (!next_value(arg, value) || !parse_int(value, options.click_world_x)) {
                std::fprintf(stderr, "glgame: invalid --click-world x\n");
                return ParseStatus::error;
            }
            if (!next_value(arg, value) || !parse_int(value, options.click_world_y)) {
                std::fprintf(stderr, "glgame: invalid --click-world y\n");
                return ParseStatus::error;
            }
            options.has_click_world = true;
        } else if (arg == "--trade-npc") {
            if (!next_value(arg, value) || value.empty()) {
                std::fputs("glgame: --trade-npc requires a person name\n", stderr);
                return ParseStatus::error;
            }
            options.trade_npc = std::string(value);
            options.has_trade_npc = true;
        } else if (arg == "--trade-buy") {
            if (!next_value(arg, value) || value.empty()) {
                std::fputs("glgame: --trade-buy requires an item tech name\n", stderr);
                return ParseStatus::error;
            }
            options.trade_hooks.push_back(TradeHook{TradeHookKind::buy, std::string(value)});
        } else if (arg == "--trade-sell") {
            if (!next_value(arg, value) || value.empty()) {
                std::fputs("glgame: --trade-sell requires an item tech name\n", stderr);
                return ParseStatus::error;
            }
            options.trade_hooks.push_back(TradeHook{TradeHookKind::sell, std::string(value)});
        } else if (arg == "--trade-exchange") {
            options.trade_hooks.push_back(TradeHook{TradeHookKind::exchange, {}});
        } else if (arg == "--dialog-answer") {
            if (!next_value(arg, value)) return ParseStatus::error;
            int answer = 0;
            if (!parse_int(value, answer) || answer < 0) {
                std::fprintf(stderr, "glgame: invalid --dialog-answer: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.dialog_answer = answer;
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

gl::game::HeroOutfit make_naked_outfit() {
    gl::game::HeroOutfit outfit;
    outfit.wear_set = "noweapon_thrw";
    outfit.weapon.clear();
    outfit.shield.clear();
    outfit.elbarms.clear();
    outfit.helmet.clear();
    outfit.armor.clear();
    return outfit;
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

std::int64_t count_item(const std::vector<gl::assets::InventoryItem>& items,
                        std::uint32_t item_id) {
    std::int64_t total = 0;
    for (const gl::assets::InventoryItem& item : items) {
        if (item.item_id == item_id) total += item.count;
    }
    return total;
}

struct DialogContext {
    gl::assets::Sdb sdb;
    gl::game::GameScriptHost host;
    std::unique_ptr<gl::script::ScriptVm> vm;
    std::unique_ptr<gl::script::DialogSession> session;
    std::string path;
    std::string speaker;
};

std::optional<gl::game::ItemTemplate>
make_item_template(const gl::assets::ItemCatalogEntry *entry) {
    if (entry == nullptr) return std::nullopt;
    std::vector<gl::game::ItemTemplateEffect> effects;
    effects.reserve(entry->item.effects.size());
    for (const gl::assets::ItemEffect &effect : entry->item.effects)
        effects.push_back({effect.type, static_cast<std::int32_t>(effect.amount), effect.flags});
    return gl::game::ItemTemplate{
        static_cast<std::uint32_t>(entry->item.type), entry->item.flags,
        entry->item.unknown_b[1], entry->item.max_stack,
        static_cast<std::uint32_t>(effects.size()), std::move(effects)};
}

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
        load_inventory_font();
        load_item_catalog();
        load_item_name_index();
        load_specials();
        rebuild_inventory_runtime();

        if (!state_.transition(gl::core::AppState::menu)) return 3;

        apply_initial_server_config();
        if (config_.get_bool("d_console", false)) set_console_visible(true);
        if (options_.has_dialog && console_visible_) {
            set_console_visible(false);
            gl::log(gl::LogLevel::info, "dialog: console disabled for --dialog");
        }

        if (options_.equip_save) load_equip_save(*options_.equip_save);

        if (options_.has_dialog) {
            rebuild_person_runtime({});
            const gl::Result<void> opened = open_dialog(options_.dialog);
            if (!opened) return exit_code_for(opened.error());
            dialog_launched_from_cli_ = true;
        } else if (options_.has_level && !options_.menu) {
            if (!load_level(level_name_)) return 3;
            if (!state_.transition(gl::core::AppState::level)) return 3;
            dump_npc_inventories();
            apply_cli_click();
            if (options_.has_trade_npc) show_trade_panel(options_.trade_npc);
        }

        apply_cli_dialog_answer();

        if (options_.equip_save && !options_.equip_save_pending)
            apply_loaded_equip_save();

        update_viewport();
        if (level_ && options_.camera_x && options_.camera_y) {
            const gl::render::Camera& camera = level_->camera();
            level_->pan(static_cast<float>(*options_.camera_x) - camera.x,
                        static_cast<float>(*options_.camera_y) - camera.y);
        }

        for (const std::string& command : options_.exec) {
            execute_line(command, false);
        }

        if (options_.inventory) open_inventory();

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
        if (options_.has_click_npc || options_.has_click_world) {
            run_headless_click();
            apply_cli_dialog_answer();
            render(0, 0);
        } else {
            render(0, options_.sim_ms);
        }
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

            if (state_.state() == gl::core::AppState::level && level_ &&
                gl::app::level_input_enabled(dialog_ != nullptr || dialog_overlay_.active(),
                                             save_slots_, console_visible_,
                                             inventory_overlay_.visible(),
                                             trade_overlay_.visible())) {
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

            const std::uint64_t animation_ms = client_.snapshot().simulation_time_ms;
            render(SDL_GetTicks64(), animation_ms);
            advance_hero_after_render(animation_ms);
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
        update_dialog_style();
    }

    void load_inventory_font() {
        if (font_) return;
        constexpr std::string_view kGameFont = "engineres/fonts/ttf/arial.ttf";
        auto bytes = vfs_.read(kGameFont);
        if (!bytes) {
            gl::log(gl::LogLevel::warning,
                    "inventory font: cannot read " + std::string(kGameFont));
            return;
        }
        auto loaded = gl::render::Font::load_from_memory(
            window_.renderer(),
            std::span<const std::uint8_t>(bytes->data(), bytes->size()),
            kFontPixelHeight, kGameFont);
        if (!loaded) {
            gl::log(gl::LogLevel::warning,
                    "inventory font: " + loaded.error().message);
            return;
        }
        inventory_font_.emplace(std::move(loaded.value()));
        gl::log(gl::LogLevel::info,
                "inventory font: loaded '" + std::string(kGameFont) + "'");
    }

    const gl::render::Font* inventory_font() const {
        if (font_) return &*font_;
        return inventory_font_ ? &*inventory_font_ : nullptr;
    }

    void load_item_catalog() {
        gl::Result<gl::assets::ItemCatalog> catalog = gl::assets::ItemCatalog::load(vfs_);
        if (!catalog) {
            gl::log(gl::LogLevel::warning,
                    "inventory: item catalog: " + catalog.error().message);
            return;
        }
        item_catalog_ = std::move(catalog.value());
        gl::log(gl::LogLevel::info, "inventory: catalog loaded");
    }

    void load_item_name_index() {
        gl::Result<gl::assets::ItemNameIndex> names = gl::assets::ItemNameIndex::load(vfs_);
        if (!names) {
            gl::log(gl::LogLevel::warning,
                    "inventory: item name index: " + names.error().message);
            return;
        }
        item_names_ = std::move(names.value());
        gl::log(gl::LogLevel::info, "inventory: item name index loaded");
    }

    void load_specials() {
        specials_.reset();
        if (options_.no_specials) {
            gl::log(gl::LogLevel::info,
                    "specials: disabled (--no-specials)");
            return;
        }
        gl::Result<gl::assets::SpecialDatabase> specials =
            gl::assets::load_specials_scr(vfs_);
        if (!specials) {
            gl::log(gl::LogLevel::warning,
                    "specials: " + specials.error().message +
                        "; using no-specials");
            return;
        }
        specials_ = std::move(specials.value());
        gl::log(gl::LogLevel::info,
                "specials: loaded " + std::to_string(specials_->defs.size()) +
                    " defs, " + std::to_string(specials_->item_classes.size()) +
                    " item classes");
    }

    const gl::assets::SpecialDatabase* specials_db() const {
        return specials_ ? &*specials_ : nullptr;
    }

    std::string dump_items(const std::vector<gl::assets::InventoryItem>& items) const {
        std::string text = "[";
        bool first = true;
        for (const gl::assets::InventoryItem& item : items) {
            if (!first) text += ',';
            first = false;
            text += std::to_string(item.item_id) + ":" +
                    std::to_string(item.count) + ":" +
                    std::to_string(item.flags) + ":" +
                    std::to_string(item.mod_count);
        }
        text += "]";
        return text;
    }

    void dump_container(std::string_view person, std::string_view container,
                        const std::vector<gl::assets::InventoryItem>& items) const {
        gl::log(gl::LogLevel::info,
                "npc dump: person='" + std::string(person) + "' container='" +
                    std::string(container) + "' items=" + dump_items(items));
        for (const gl::assets::InventoryItem& item : items) {
            for (const gl::assets::SpecialMod& mod : item.mods) {
                gl::log(gl::LogLevel::info,
                        "npc mod: person='" + std::string(person) + "' container='" +
                            std::string(container) + "' item=" +
                            std::to_string(item.item_id) + " mod=" +
                            std::to_string(mod.id) + " value=" +
                            std::to_string(mod.value) + " flags=" +
                            std::to_string(mod.flags));
            }
        }
    }

    void dump_npc_inventories() const {
        for (const std::string& person : options_.dump_npcs) {
            dump_container(person, "main", inventory_runtime_.person_items(person));
            dump_container(person, "trade", inventory_runtime_.trade_items(person));
        }
    }

    void rebuild_inventory_runtime() {
        const gl::assets::HeroInventory inventory =
            pending_inventory_ ? *pending_inventory_ : gl::assets::HeroInventory{};
        const auto item_template =
            [this](std::uint32_t id) -> std::optional<gl::game::ItemTemplate> {
            const gl::assets::ItemCatalogEntry* entry = item_catalog_.find(id);
            return make_item_template(entry);
        };
        if (item_names_) {
            inventory_runtime_ =
                gl::game::InventoryRuntime::create(inventory, *item_names_, item_template);
        } else {
            inventory_runtime_ = gl::game::InventoryRuntime::create(
                inventory, gl::assets::ItemNameIndex{}, item_template);
        }
        gl::log(gl::LogLevel::info,
                "inventory runtime: Hero money=" +
                    std::to_string(inventory_runtime_.money()));
    }

    std::int32_t loot_level() const {
        if (!pending_hero_stats_)
            return 1;
        return gl::game::hero_level_from_experience(pending_hero_stats_->experience) + 1;
    }

    void generate_npc_inventories(const gl::game::Level& level, gl::game::MsvcRandom& rng) {
        const gl::assets::ItemNameIndex empty_names;
        const gl::assets::ItemNameIndex& names = item_names_ ? *item_names_ : empty_names;
        const auto item_template =
            [this](std::uint32_t id) -> std::optional<gl::game::ItemTemplate> {
            const gl::assets::ItemCatalogEntry* entry = item_catalog_.find(id);
            return make_item_template(entry);
        };
        gl::game::generate_npc_inventories(inventory_runtime_, level.persons, vfs_, names,
                                           item_template, loot_level(), rng,
                                           specials_db());
        std::size_t populated = 0;
        for (const gl::game::PersonInstance& person : level.persons) {
            if (!inventory_runtime_.person_items(person.name).empty())
                ++populated;
        }
        gl::log(gl::LogLevel::info,
                "npc inventory: level '" + level.name + "', loot level " +
                    std::to_string(loot_level()) + ", populated " +
                    std::to_string(populated) + "/" +
                    std::to_string(level.persons.size()));
    }

    void open_inventory() {
        const gl::assets::HeroInventory& inventory = inventory_runtime_.hero_inventory();
        inventory_overlay_.open(vfs_, item_catalog_, inventory);
        inventory_overlay_.set_font(inventory_font());
        if (options_.inventory_select) {
            if (!inventory_overlay_.select_visible_slot(
                    static_cast<std::size_t>(*options_.inventory_select))) {
                gl::log(gl::LogLevel::warning,
                        "inventory: selected slot " + std::to_string(*options_.inventory_select) +
                            " is empty");
            }
        }
        gl::log(gl::LogLevel::info,
                "inventory: opened hot=" + std::to_string(inventory.hot.size()) + " items=" +
                    std::to_string(inventory.items.size()) + " equipped=" +
                    std::to_string(inventory.equipped.size()));
    }

    void toggle_inventory() {
        if (inventory_overlay_.visible()) {
            inventory_overlay_.close();
            gl::log(gl::LogLevel::info, "inventory: closed");
        } else {
            open_inventory();
        }
    }

    void show_trade_panel(std::string_view npc) {
        const std::vector<gl::assets::InventoryItem>& npc_items =
            inventory_runtime_.trade_items(npc);
        if (npc_items.empty())
            gl::log(gl::LogLevel::warning,
                    "trade: person '" + std::string(npc) + "' has no trade items");
        const std::int32_t npc_money = inventory_runtime_.trade_money(npc);
        const std::vector<gl::assets::InventoryItem>& hero_items =
            inventory_runtime_.hero_inventory().items;
        const std::int32_t hero_money = inventory_runtime_.money();
        current_trade_npc_ = std::string(npc);
        trade_overlay_.open(vfs_, item_catalog_, current_trade_npc_, npc_items, npc_money,
                            hero_items, hero_money);
        trade_overlay_.set_font(inventory_font());
        trade_overlay_.set_price_provider(
            [this](const gl::assets::InventoryItem& item)
                -> std::optional<gl::game::TradePrice> {
                const gl::assets::ItemCatalogEntry* entry = item_catalog_.find(item.item_id);
                if (entry == nullptr)
                    return std::nullopt;
                const double skill = person_runtime_.get_skill("Hero", "SKILL_TRADE");
                gl::game::TradePriceInput input{};
                input.item_id = item.item_id;
                input.item_price = entry->item.price;
                input.durability = item.durability;
                input.flags = item.flags;
                input.mod_count = item.mod_count;
                input.skill_trade =
                    static_cast<std::uint32_t>(std::max(0.0, std::min(skill, 15.0)));
                input.count = item.count;
                input.assume_pc53 = true;
                return gl::game::compute_trade_price(input);
            });
        trade_overlay_.set_exchange_handler(
            [this](const std::vector<gl::game::TradePrice>& buy_items,
                   const std::vector<gl::game::TradePrice>& sell_items) {
                gl::app::TradeExchangeResult result;
                result.status = inventory_runtime_.exchange_trade(current_trade_npc_, "Hero",
                                                                  buy_items, sell_items);
                if (result.status == gl::game::TradeStatus::ok) {
                    result.snapshot.npc_items =
                        inventory_runtime_.trade_items(current_trade_npc_);
                    result.snapshot.hero_items = inventory_runtime_.hero_inventory().items;
                    result.snapshot.npc_money = inventory_runtime_.trade_money(current_trade_npc_);
                    result.snapshot.hero_money = inventory_runtime_.money();
                }
                return result;
            });
        gl::log(gl::LogLevel::info, "trade: opened npc='" + current_trade_npc_ +
                                        "' npc_items=" + std::to_string(npc_items.size()) +
                                        " npc_money=" + std::to_string(npc_money) +
                                        " hero_items=" + std::to_string(hero_items.size()) +
                                        " hero_money=" + std::to_string(hero_money));
        run_trade_hooks();
    }

    void request_trade_panel() {
        if (!dialog_ || dialog_->speaker.empty()) {
            gl::log(gl::LogLevel::warning,
                    "trade: RS_PassToTradePanel without an active dialog speaker");
            return;
        }
        pending_trade_npc_ = dialog_->speaker;
    }

    void maybe_open_pending_trade_panel() {
        if (!pending_trade_npc_) return;
        const std::string npc = std::move(*pending_trade_npc_);
        pending_trade_npc_.reset();
        close_dialog();
        show_trade_panel(npc);
    }

    std::optional<std::uint32_t> trade_item_id(std::string_view tech) const {
        if (item_names_) {
            const std::optional<std::uint32_t> id = item_names_->find(tech);
            if (id) return id;
        }
        std::uint32_t parsed = 0;
        const auto [ptr, error] =
            std::from_chars(tech.data(), tech.data() + tech.size(), parsed);
        if (error == std::errc{} && ptr == tech.data() + tech.size()) return parsed;
        return std::nullopt;
    }

    void run_trade_hooks() {
        if (options_.trade_hooks.empty() || trade_hooks_run_) return;
        if (!trade_overlay_.visible()) {
            gl::log(gl::LogLevel::warning,
                    "trade: --trade-* hooks ignored (no trade panel open)");
            return;
        }
        trade_hooks_run_ = true;
        std::vector<std::uint32_t> touched;
        const auto remember = [&touched](std::uint32_t id) {
            if (std::find(touched.begin(), touched.end(), id) == touched.end())
                touched.push_back(id);
        };
        for (const TradeHook& hook : options_.trade_hooks) {
            if (hook.kind == TradeHookKind::exchange) {
                const std::int64_t buy_total = trade_overlay_.buy_total();
                const std::int64_t sell_total = trade_overlay_.sell_total();
                const gl::game::TradeStatus status = trade_overlay_.run_exchange();
                const bool ok = status == gl::game::TradeStatus::ok;
                const std::string totals =
                    " net=" + std::to_string(sell_total - buy_total) +
                    " buy=" + std::to_string(buy_total) +
                    " sell=" + std::to_string(sell_total) +
                    " npc_money=" + std::to_string(trade_overlay_.npc_money()) +
                    " hero_money=" + std::to_string(trade_overlay_.hero_money());
                if (ok) {
                    gl::log(gl::LogLevel::info, "trade: hook exchange result=ok" + totals);
                } else {
                    gl::log(gl::LogLevel::info,
                            "trade: hook exchange result=failed" + totals + " error='" +
                                trade_overlay_.last_error() + "'");
                }
                for (const std::uint32_t id : touched) {
                    gl::log(gl::LogLevel::info,
                            "trade: hook stock id=" + std::to_string(id) +
                                " npc=" +
                                std::to_string(count_item(
                                    inventory_runtime_.trade_items(current_trade_npc_), id)) +
                                " hero=" +
                                std::to_string(count_item(
                                    inventory_runtime_.hero_inventory().items, id)));
                }
                continue;
            }
            const bool buy = hook.kind == TradeHookKind::buy;
            const gl::app::TradeOrigin origin =
                buy ? gl::app::TradeOrigin::npc : gl::app::TradeOrigin::hero;
            const std::optional<std::uint32_t> item_id = trade_item_id(hook.tech);
            if (!item_id) {
                gl::log(gl::LogLevel::warning, "trade: hook " +
                                                   std::string(buy ? "buy" : "sell") +
                                                   " unknown item tech '" + hook.tech + "'");
                continue;
            }
            remember(*item_id);
            const std::vector<gl::assets::InventoryItem>& source =
                buy ? trade_overlay_.npc_items() : trade_overlay_.hero_items();
            const std::int64_t count = count_item(source, *item_id);
            const bool staged = trade_overlay_.stage_item_by_id(origin, *item_id);
            if (!staged) {
                gl::log(gl::LogLevel::info,
                        "trade: hook stage " + std::string(buy ? "buy" : "sell") +
                            " id=" + std::to_string(*item_id) + " count=" +
                            std::to_string(count) + " result=failed error='" +
                            trade_overlay_.last_error() + "'");
                continue;
            }
            const std::vector<gl::app::StagedTradeItem>& staged_items =
                buy ? trade_overlay_.buy_staged_items() : trade_overlay_.sell_staged_items();
            gl::game::TradePrice price;
            if (!staged_items.empty()) price = staged_items.back().price;
            gl::log(gl::LogLevel::info,
                    "trade: hook stage " + std::string(buy ? "buy" : "sell") +
                        " id=" + std::to_string(price.item_id) + " count=" +
                        std::to_string(price.count) + " buy=" + std::to_string(price.buy) +
                        " sell=" + std::to_string(price.sell) + " result=ok");
        }
    }

    gl::Result<void> open_dialog(std::string_view virtual_path, std::string_view speaker = {}) {
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
        context->host.services().console = [](std::string_view text) {
            gl::log(gl::LogLevel::info, "script console: " + std::string(text));
        };
        context->host.services().command = [this](std::string_view text) {
            const auto result = registry_.execute(text);
            if (!result) gl::log(gl::LogLevel::warning, "script cmd: " + result.error().message);
        };
        context->host.services().signal = [](std::string_view text) {
            gl::log(gl::LogLevel::info, "script signal: " + std::string(text));
        };
        context->host.services().play_sound = [](std::string_view name) {
            gl::log(gl::LogLevel::debug, "script sound: " + std::string(name));
        };
        context->host.services().exit = [](std::int32_t code) {
            gl::log(gl::LogLevel::info, "script exit: " + std::to_string(code));
        };
        context->host.services().person = person_runtime_.access();
        context->host.services().inventory = inventory_runtime_.access();
        context->host.services().trade_panel = [this]() { request_trade_panel(); };
        context->path = std::string(virtual_path);
        context->speaker = std::string(speaker);
        context->sdb = std::move(sdb.value());
        context->vm =
            std::make_unique<gl::script::ScriptVm>(std::move(nodes.value()), context->host);
        context->vm->set_globals(globals.value());
        context->session = std::make_unique<gl::script::DialogSession>(*context->vm, context->sdb);

        dialog_ = std::move(context);
        const gl::Result<void> started = dialog_->session->start();
        if (!started) {
            gl::log(gl::LogLevel::error,
                    "dialog '" + dialog_->path + "': " + started.error().message);
            reset_dialog();
            return started.error();
        }

        if (pending_trade_npc_) {
            maybe_open_pending_trade_panel();
            return {};
        }

        refresh_dialog_overlay();
        gl::log(gl::LogLevel::info,
                "dialog: '" + dialog_->path + "' speaker='" + dialog_->speaker + "' answers=" +
                    std::to_string(dialog_->session->answers().size()) + " text_len=" +
                    std::to_string(dialog_->session->npc_text().size()));
        return {};
    }

    void close_dialog() {
        if (dialog_)
            gl::log(gl::LogLevel::info, "dialog: closed '" + dialog_->path + "'");
        reset_dialog();
    }

    void reset_dialog() {
        dialog_.reset();
        dialog_overlay_.clear();
        pending_trade_npc_.reset();
        pending_talk_.reset();
        pending_talk_tile_.reset();
    }

    void refresh_dialog_overlay() {
        if (!dialog_ || !dialog_->session) {
            dialog_overlay_.clear();
            return;
        }
        std::vector<std::string> answers;
        if (dialog_->session->waiting()) {
            const std::span<const gl::script::DialogLine> lines = dialog_->session->answers();
            answers.reserve(lines.size());
            for (const gl::script::DialogLine& line : lines) answers.push_back(line.text);
        }
        dialog_overlay_.set_content(dialog_->speaker,
                                    std::string(dialog_->session->npc_text()),
                                    std::move(answers));
    }

    void update_dialog_style() {
        gl::app::DialogOverlayStyle style;
        style.viewport_width = viewport_width_;
        style.viewport_height = viewport_height_;
        style.margin = kDialogMargin;
        style.line_height = ui_line_height();
        style.line_gap = 2.0f;
        style.answer_gap = kDialogAnswerGap;
        style.answer_row_height = std::max(ui_line_height() + kDialogAnswerGap, 36.0f);
        style.fallback_text_scale = kScale;
        dialog_overlay_.set_style(style);
        dialog_overlay_.set_font(font_ ? &*font_ : nullptr);
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

    void dialog_move(int delta) {
        if (!dialog_waiting()) return;
        dialog_overlay_.move_selection(delta);
    }

    void dialog_activate() {
        if (!dialog_waiting()) return;
        const std::span<const gl::script::DialogLine> answers = dialog_->session->answers();
        const std::size_t selected = dialog_overlay_.selected();
        if (answers.empty() || selected >= answers.size()) return;
        const gl::Result<void> chosen = dialog_->session->choose(selected);
        if (!chosen) {
            gl::log(gl::LogLevel::error, "dialog: " + chosen.error().message);
            pending_trade_npc_.reset();
            return;
        }
        if (pending_trade_npc_) {
            maybe_open_pending_trade_panel();
            return;
        }
        refresh_dialog_overlay();
    }

    void apply_cli_dialog_answer() {
        if (!options_.dialog_answer || !dialog_waiting()) return;
        const std::size_t index = static_cast<std::size_t>(*options_.dialog_answer);
        const std::span<const gl::script::DialogLine> answers = dialog_->session->answers();
        if (index >= answers.size()) {
            gl::log(gl::LogLevel::warning, "dialog: --dialog-answer " + std::to_string(index) +
                                               " is out of range (" +
                                               std::to_string(answers.size()) + " answers)");
            return;
        }
        dialog_overlay_.set_selected(index);
        dialog_activate();
    }

    void dialog_hover(int x, int y) {
        if (!dialog_waiting()) return;
        const std::optional<std::size_t> index = dialog_overlay_.hit_test(x, y);
        if (index) dialog_overlay_.set_selected(*index);
    }

    void dialog_click(int x, int y) {
        if (!dialog_waiting()) return;
        const std::optional<std::size_t> index = dialog_overlay_.hit_test(x, y);
        if (!index) return;
        dialog_overlay_.set_selected(*index);
        dialog_activate();
    }

    bool can_interact_person(std::size_t index) {
        if (!level_) return false;
        const std::vector<gl::game::PersonInstance>& persons = level_->level().persons;
        const std::vector<gl::game::PersonMovement>& movements =
            level_->simulation().persons();
        if (index >= persons.size() || index >= movements.size()) return false;
        return level_->hero().can_interact(persons[index], movements[index]);
    }

    std::optional<std::string> talk_script_for(std::size_t index) const {
        if (!level_) return std::nullopt;
        const std::vector<gl::game::PersonInstance>& persons = level_->level().persons;
        if (index >= persons.size()) return std::nullopt;
        std::optional<std::string> script = gl::game::dialog_script_path(persons[index]);
        if (!script) return std::nullopt;
        if (!vfs_.exists(*script)) {
            gl::log(gl::LogLevel::warning,
                    "person '" + persons[index].name + "': missing dialog script '" +
                        *script + "'");
            return std::nullopt;
        }
        return script;
    }

    void handle_person_click(std::size_t index) {
        if (!level_) return;
        const std::vector<gl::game::PersonInstance>& persons = level_->level().persons;
        const std::vector<gl::game::PersonMovement>& movements =
            level_->simulation().persons();
        if (index >= persons.size() || index >= movements.size()) {
            gl::log(gl::LogLevel::warning,
                    "click: invalid person index " + std::to_string(index));
            return;
        }
        const gl::game::PersonInstance& person = persons[index];
        const std::optional<std::string> script = talk_script_for(index);
        const gl::app::PersonClickAction action =
            gl::app::resolve_person_click(script.has_value(), can_interact_person(index));
        if (action == gl::app::PersonClickAction::ignore) {
            pending_talk_.reset();
            pending_talk_tile_.reset();
            gl::log(gl::LogLevel::warning,
                    "person '" + person.name + "': not talkable, click ignored");
            return;
        }
        if (action == gl::app::PersonClickAction::open) {
            pending_talk_.reset();
            pending_talk_tile_.reset();
            const gl::Result<void> opened = open_dialog(*script, person.name);
            if (!opened) {
                gl::log(gl::LogLevel::warning,
                        "person '" + person.name + "': cannot open dialog: " +
                            opened.error().message);
            }
            return;
        }
        const gl::game::Tile target = gl::app::current_person_tile(movements[index]);
        if (!level_->hero().set_goal(target)) {
            pending_talk_.reset();
            pending_talk_tile_.reset();
            gl::log(gl::LogLevel::warning,
                    "person '" + person.name + "': cannot path to the current NPC tile");
            return;
        }
        pending_talk_ = index;
        pending_talk_tile_ = target;
    }

    void handle_level_click(int x, int y) {
        if (!level_) return;
        const gl::app::ClickTarget target = level_->click_target(x, y);
        if (target.person) {
            handle_person_click(*target.person);
            return;
        }
        pending_talk_.reset();
        pending_talk_tile_.reset();
        if (target.tile && !level_->hero().set_goal(*target.tile)) {
            gl::log(gl::LogLevel::warning, "cannot path to the clicked tile");
        }
    }

    void maybe_open_pending_dialog() {
        if (!pending_talk_ || !level_) return;
        const std::size_t index = *pending_talk_;
        const std::vector<gl::game::PersonInstance>& persons = level_->level().persons;
        const std::vector<gl::game::PersonMovement>& movements =
            level_->simulation().persons();
        if (index >= persons.size() || index >= movements.size()) {
            pending_talk_.reset();
            pending_talk_tile_.reset();
            return;
        }
        const gl::game::Tile npc_tile = gl::app::current_person_tile(movements[index]);
        const bool npc_moved = !pending_talk_tile_ || npc_tile != *pending_talk_tile_;
        const gl::app::PendingTalkDecision decision = gl::app::resolve_pending_talk(
            can_interact_person(index), level_->hero().hero().has_goal, npc_moved);
        if (decision == gl::app::PendingTalkDecision::abandon) {
            pending_talk_.reset();
            pending_talk_tile_.reset();
            return;
        }
        if (decision == gl::app::PendingTalkDecision::wait) return;
        if (decision == gl::app::PendingTalkDecision::retarget) {
            if (!level_->hero().set_goal(npc_tile)) {
                gl::log(gl::LogLevel::warning,
                        "person '" + persons[index].name +
                            "': moved to an unreachable tile, pending talk cleared");
                pending_talk_.reset();
                pending_talk_tile_.reset();
                return;
            }
            pending_talk_tile_ = npc_tile;
            return;
        }
        const std::optional<std::string> script = talk_script_for(index);
        pending_talk_.reset();
        pending_talk_tile_.reset();
        if (!script) return;
        const gl::Result<void> opened = open_dialog(*script, persons[index].name);
        if (!opened) {
            gl::log(gl::LogLevel::warning,
                    "person '" + persons[index].name + "': cannot open dialog: " +
                        opened.error().message);
        }
    }

    void apply_cli_click() {
        if (!level_) return;
        if (options_.has_click_npc) {
            handle_person_click(static_cast<std::size_t>(options_.click_npc));
        } else if (options_.has_click_world) {
            pending_talk_.reset();
            pending_talk_tile_.reset();
            if (!level_->hero().set_goal(
                    gl::game::Tile{options_.click_world_x, options_.click_world_y})) {
                gl::log(gl::LogLevel::warning, "cannot path to the clicked world tile");
            }
        }
    }

    void advance_scene(double dt_seconds) {
        if (!level_) return;
        level_->simulation().advance(dt_seconds);
        level_->hero().advance(dt_seconds);
        maybe_open_pending_dialog();
    }

    void run_headless_click() {
        if (!level_ || dialog_ || trade_overlay_.visible()) return;
        constexpr double kStepSeconds = 0.05;
        const double limit_seconds = options_.sim_ms > 0
                                         ? static_cast<double>(options_.sim_ms) / 1000.0
                                         : 60.0;
        for (double elapsed = 0.0; elapsed < limit_seconds; elapsed += kStepSeconds) {
            advance_scene(kStepSeconds);
            if (dialog_ || trade_overlay_.visible() || !level_->hero().hero().has_goal) break;
        }
    }

    void advance_hero_after_render(std::uint64_t animation_ms) {
        if (!(state_.state() == gl::core::AppState::level && level_)) {
            level_animation_ms_ = animation_ms;
            return;
        }
        const std::uint64_t delta =
            animation_ms > level_animation_ms_ ? animation_ms - level_animation_ms_ : 0;
        level_animation_ms_ = animation_ms;
        if (dialog_ || delta == 0) return;
        level_->hero().advance(static_cast<double>(delta) / 1000.0);
        maybe_open_pending_dialog();
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

        registry_.add("inventory", "toggle the inventory overlay",
                      [this](gl::core::Args args) -> gl::Result<std::string> {
                          if (!args.empty()) {
                              return gl::Error{gl::ErrorCode::argument,
                                               "usage: inventory"};
                          }
                          toggle_inventory();
                          return std::string{};
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

    bool read_save_body(const gl::core::SaveSlotInfo& slot, std::vector<std::uint8_t>& body,
                        std::string& error) {
        error.clear();
        std::ifstream in(slot.path, std::ios::binary);
        if (!in) {
            error = "cannot open '" + slot.path.string() + "'";
            return false;
        }
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        auto decoded = gl::assets::decode_save_body(bytes);
        if (!decoded) {
            error = decoded.error().message;
            return false;
        }
        body = std::move(decoded.value());
        return true;
    }

    std::optional<gl::game::HeroOutfit> resolve_outfit_from_equipment(
        const gl::assets::HeroEquipment& equipment, std::string& error) {
        error.clear();
        auto sdb_data = vfs_.read("sdb/items/tech_names.sdb");
        if (!sdb_data) {
            error = sdb_data.error().message;
            return std::nullopt;
        }
        auto tech_names = gl::assets::Sdb::parse(sdb_data.value());
        if (!tech_names) {
            error = tech_names.error().message;
            return std::nullopt;
        }

        auto wear_data = vfs_.read("scripts/herowear.scr");
        if (!wear_data) {
            error = wear_data.error().message;
            return std::nullopt;
        }
        auto wear = gl::assets::parse_herowear(wear_data.value());
        if (!wear) {
            error = wear.error().message;
            return std::nullopt;
        }

        auto outfit = gl::game::resolve_hero_outfit(equipment, tech_names.value(), wear.value());
        if (!outfit) {
            error = outfit.error().message;
            return std::nullopt;
        }
        return outfit.value();
    }

    void report_outfit_message(const std::string& message, bool info) {
        save_message_ = message;
        console_print(message);
        gl::log(info ? gl::LogLevel::info : gl::LogLevel::warning, message);
    }

    void load_equip_save(int slot) {
        pending_equipment_.reset();
        pending_inventory_.reset();
        pending_hero_stats_.reset();
        pending_outfit_.reset();

        const gl::core::SaveSlotInfo* info = saves_.find(slot);
        gl::core::SaveSlotInfo synthetic;
        if (info == nullptr) {
            synthetic.slot = slot;
            synthetic.path = options_.game_dir / "Saves" /
                             ("slot" + std::to_string(slot) + ".sav");
            info = &synthetic;
        }

        std::error_code path_error;
        if (!fs::exists(info->path, path_error)) {
            pending_equipment_ = gl::assets::HeroEquipment{};
            pending_inventory_ = gl::assets::HeroInventory{};
            pending_outfit_ = make_naked_outfit();
            rebuild_inventory_runtime();
            report_outfit_message(
                "outfit: naked (invalid save slot " + std::to_string(slot) + ")", false);
            return;
        }

        std::vector<std::uint8_t> body;
        std::string error;
        if (!read_save_body(*info, body, error)) {
            pending_equipment_ = gl::assets::HeroEquipment{};
            pending_inventory_ = gl::assets::HeroInventory{};
            pending_outfit_ = make_naked_outfit();
            rebuild_inventory_runtime();
            report_outfit_message(
                "outfit: naked (slot " + std::to_string(slot) + "): " + error, false);
            return;
        }

        auto hero_stats = gl::assets::parse_hero_stats(body);
        if (hero_stats) {
            pending_hero_stats_ = hero_stats.value();
        } else {
            gl::log(gl::LogLevel::warning, "hero stats: " + hero_stats.error().message);
        }

        auto inventory = gl::assets::parse_hero_inventory(body);
        if (inventory) {
            pending_inventory_ = std::move(inventory.value());
            gl::log(gl::LogLevel::info,
                    "inventory: slot " + std::to_string(slot) + " hot=" +
                        std::to_string(pending_inventory_->hot.size()) + " items=" +
                        std::to_string(pending_inventory_->items.size()) + " equipped=" +
                        std::to_string(pending_inventory_->equipped.size()));
        } else {
            pending_inventory_ = gl::assets::HeroInventory{};
            gl::log(gl::LogLevel::warning,
                    "inventory: slot " + std::to_string(slot) + ": " +
                        inventory.error().message);
        }
        rebuild_inventory_runtime();

        auto equipment = gl::assets::parse_hero_equipment(body);
        if (!equipment) {
            pending_equipment_ = gl::assets::HeroEquipment{};
            pending_outfit_ = make_naked_outfit();
            report_outfit_message(
                "outfit: naked (slot " + std::to_string(slot) + "): " +
                    equipment.error().message,
                false);
            return;
        }
        pending_equipment_ = std::move(equipment.value());

        if (options_.hero_outfit_none) {
            pending_outfit_ = make_naked_outfit();
            report_outfit_message("outfit: slot " + std::to_string(slot) +
                                      " ignored (--hero-outfit none)",
                                  true);
            return;
        }

        std::string resolve_error;
        auto outfit = resolve_outfit_from_equipment(*pending_equipment_, resolve_error);
        if (!outfit) {
            pending_outfit_ = make_naked_outfit();
            report_outfit_message(
                "outfit: naked (slot " + std::to_string(slot) + "): " + resolve_error, false);
            return;
        }
        pending_outfit_ = std::move(outfit.value());
        report_outfit_message("outfit: slot " + std::to_string(slot), true);
    }

    void apply_loaded_equip_save() {
        if (!level_) return;
        const gl::game::HeroOutfit outfit =
            pending_outfit_ ? *pending_outfit_ : make_naked_outfit();
        level_->apply_hero_outfit(outfit);
    }

    void rebuild_person_runtime(const std::vector<gl::game::PersonInstance>& persons) {
        person_runtime_ = gl::game::PersonRuntime::create(persons, pending_hero_stats_);
        if (!pending_hero_stats_ && !hero_stats_absent_logged_) {
            hero_stats_absent_logged_ = true;
            gl::log(gl::LogLevel::info,
                    "person runtime: Hero stats absent (no save); using zeros");
        }
    }

    bool load_level(std::string_view name) {
        gl::game::HeroOutfit hero_outfit;
        if (options_.hero_outfit_none) {
            hero_outfit = make_naked_outfit();
        } else if (pending_equipment_) {
            std::string error;
            auto resolved = resolve_outfit_from_equipment(*pending_equipment_, error);
            if (resolved) {
                hero_outfit = *resolved;
            } else {
                const std::string message = "outfit: naked: " + error;
                gl::log(gl::LogLevel::warning, message);
                console_print(message);
                hero_outfit = make_naked_outfit();
            }
        }
        auto screen = gl::app::LevelScreen::load(
            window_.renderer(), vfs_, name, !options_.no_persons, &hero_outfit, options_.seed,
            [this](const gl::game::Level& level, gl::game::MsvcRandom& rng) {
                generate_npc_inventories(level, rng);
            });
        if (!screen) {
            gl::log(gl::LogLevel::error, screen.error().message);
            console_print("cannot load level: " + screen.error().message);
            return false;
        }
        level_.emplace(std::move(screen.value()));
        level_name_ = std::string(name);
        level_animation_ms_ = 0;
        pending_talk_.reset();
        pending_talk_tile_.reset();
        update_viewport();
        rebuild_person_runtime(level_->level().persons);
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
        update_dialog_style();
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
            case SDL_FINGERDOWN: {
                if (finger_id_ != -1) return;
                if (!inventory_overlay_.visible() && !trade_overlay_.visible()) return;
                finger_id_ = event.tfinger.fingerId;
                finger_start_x_ = static_cast<int>(
                    std::lround(event.tfinger.x * static_cast<float>(viewport_width_)));
                finger_start_y_ = static_cast<int>(
                    std::lround(event.tfinger.y * static_cast<float>(viewport_height_)));
                finger_last_x_ = finger_start_x_;
                finger_last_y_ = finger_start_y_;
                finger_scroll_accum_ = 0.0f;
                finger_dragged_ = false;
                if (trade_overlay_.visible())
                    trade_overlay_.handle_mouse(finger_start_x_, finger_start_y_, false);
                else
                    inventory_overlay_.handle_mouse(finger_start_x_, finger_start_y_, false);
                return;
            }
            case SDL_FINGERMOTION: {
                if (finger_id_ != event.tfinger.fingerId) return;
                if (!inventory_overlay_.visible() && !trade_overlay_.visible()) return;
                const int x = static_cast<int>(
                    std::lround(event.tfinger.x * static_cast<float>(viewport_width_)));
                const int y = static_cast<int>(
                    std::lround(event.tfinger.y * static_cast<float>(viewport_height_)));
                const int dx = x - finger_start_x_;
                const int dy = y - finger_start_y_;
                if (dx * dx + dy * dy >= 16 * 16) finger_dragged_ = true;
                finger_scroll_accum_ += static_cast<float>(finger_last_y_ - y);
                finger_last_x_ = x;
                finger_last_y_ = y;
                constexpr float kFingerScrollStep = 24.0f;
                const bool trade = trade_overlay_.visible();
                while (finger_scroll_accum_ >= kFingerScrollStep) {
                    if (trade)
                        trade_overlay_.handle_wheel(1, finger_start_x_, finger_start_y_);
                    else
                        inventory_overlay_.handle_wheel(1, finger_start_x_, finger_start_y_);
                    finger_scroll_accum_ -= kFingerScrollStep;
                }
                while (finger_scroll_accum_ <= -kFingerScrollStep) {
                    if (trade)
                        trade_overlay_.handle_wheel(-1, finger_start_x_, finger_start_y_);
                    else
                        inventory_overlay_.handle_wheel(-1, finger_start_x_, finger_start_y_);
                    finger_scroll_accum_ += kFingerScrollStep;
                }
                return;
            }
            case SDL_FINGERUP: {
                if (finger_id_ != event.tfinger.fingerId) return;
                if ((inventory_overlay_.visible() || trade_overlay_.visible()) &&
                    !finger_dragged_) {
                    const int x = static_cast<int>(
                        std::lround(event.tfinger.x * static_cast<float>(viewport_width_)));
                    const int y = static_cast<int>(
                        std::lround(event.tfinger.y * static_cast<float>(viewport_height_)));
                    if (trade_overlay_.visible())
                        trade_overlay_.handle_mouse(x, y, true);
                    else
                        inventory_overlay_.handle_mouse(x, y, true);
                }
                finger_id_ = -1;
                finger_dragged_ = false;
                finger_scroll_accum_ = 0.0f;
                return;
            }
            case SDL_MOUSEMOTION:
                if (console_visible_) return;
                if (trade_overlay_.visible()) {
                    if (event.motion.which != SDL_TOUCH_MOUSEID)
                        trade_overlay_.handle_mouse(event.motion.x, event.motion.y, false);
                    return;
                }
                if (inventory_overlay_.visible()) {
                    if (event.motion.which != SDL_TOUCH_MOUSEID)
                        inventory_overlay_.handle_mouse(event.motion.x, event.motion.y, false);
                    return;
                }
                if (dialog_) {
                    dialog_hover(event.motion.x, event.motion.y);
                } else if (save_slots_) {
                    save_hover(event.motion.x, event.motion.y);
                } else if (state_.state() == gl::core::AppState::menu) {
                    menu_hover(event.motion.x, event.motion.y);
                } else if (state_.state() == gl::core::AppState::level && mouse_down_ &&
                           level_) {
                    const int dx = event.motion.x - mouse_down_x_;
                    const int dy = event.motion.y - mouse_down_y_;
                    if (!dragging_ && dx * dx + dy * dy >= 16) dragging_ = true;
                    if (dragging_) {
                        const float zoom = level_->camera().zoom;
                        level_->pan(-static_cast<float>(event.motion.xrel) / zoom,
                                    -static_cast<float>(event.motion.yrel) / zoom);
                    }
                }
                return;
            case SDL_MOUSEBUTTONDOWN:
                if (console_visible_) return;
                if (trade_overlay_.visible()) {
                    if (event.button.which != SDL_TOUCH_MOUSEID)
                        trade_overlay_.handle_mouse(event.button.x, event.button.y,
                                                    event.button.button == SDL_BUTTON_LEFT);
                    return;
                }
                if (inventory_overlay_.visible()) {
                    if (event.button.which != SDL_TOUCH_MOUSEID)
                        inventory_overlay_.handle_mouse(event.button.x, event.button.y,
                                                        event.button.button == SDL_BUTTON_LEFT);
                    return;
                }
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (!dialog_ && !save_slots_ &&
                        state_.state() == gl::core::AppState::level && level_) {
                        level_->hero().clear_goal();
                        pending_talk_.reset();
                        pending_talk_tile_.reset();
                    }
                    return;
                }
                if (event.button.button != SDL_BUTTON_LEFT) return;
                if (dialog_) {
                    dialog_click(event.button.x, event.button.y);
                } else if (save_slots_) {
                    save_click(event.button.x, event.button.y);
                } else if (state_.state() == gl::core::AppState::menu) {
                    menu_click(event.button.x, event.button.y);
                } else if (state_.state() == gl::core::AppState::level) {
                    mouse_down_ = true;
                    mouse_down_x_ = event.button.x;
                    mouse_down_y_ = event.button.y;
                    dragging_ = false;
                }
                return;
            case SDL_MOUSEBUTTONUP:
                if (trade_overlay_.visible() || inventory_overlay_.visible()) {
                    mouse_down_ = false;
                    dragging_ = false;
                    return;
                }
                if (event.button.button != SDL_BUTTON_LEFT) return;
                if (mouse_down_ && !dialog_ && !save_slots_ && !console_visible_ &&
                    state_.state() == gl::core::AppState::level && level_) {
                    const int dx = event.button.x - mouse_down_x_;
                    const int dy = event.button.y - mouse_down_y_;
                    if (!dragging_ && dx * dx + dy * dy < 16) {
                        handle_level_click(event.button.x, event.button.y);
                    }
                }
                mouse_down_ = false;
                dragging_ = false;
                return;
            case SDL_MOUSEWHEEL: {
                if (console_visible_ || event.wheel.y == 0) {
                    return;
                }
                int mouse_x = 0;
                int mouse_y = 0;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                if (trade_overlay_.visible()) {
                    trade_overlay_.handle_wheel(event.wheel.y, mouse_x, mouse_y);
                    return;
                }
                if (state_.state() != gl::core::AppState::level || !level_) return;
                if (inventory_overlay_.visible()) {
                    inventory_overlay_.handle_wheel(event.wheel.y, mouse_x, mouse_y);
                    return;
                }
                if (!gl::app::level_input_enabled(dialog_ != nullptr || dialog_overlay_.active(),
                                                  save_slots_, console_visible_,
                                                  inventory_overlay_.visible(),
                                                  trade_overlay_.visible())) {
                    return;
                }
                const float factor = event.wheel.y > 0 ? kZoomStep : 1.0f / kZoomStep;
                level_->zoom_at(factor, static_cast<float>(mouse_x),
                                static_cast<float>(mouse_y));
                return;
            }
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    update_viewport();
                } else if (event.window.event == SDL_WINDOWEVENT_LEAVE ||
                           event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    mouse_down_ = false;
                    dragging_ = false;
                }
                return;
            default:
                return;
        }
    }

    void handle_keydown(const SDL_KeyboardEvent& key) {
        const SDL_Keycode raw_symbol = key.keysym.sym;
        const SDL_Keycode symbol =
            raw_symbol == SDLK_AC_BACK ? SDLK_ESCAPE : raw_symbol;
        if (console_visible_) {
            handle_console_key(key);
            return;
        }
        if (trade_overlay_.visible()) {
            if (key.repeat == 0) trade_overlay_.handle_key(symbol);
            return;
        }
        if (inventory_overlay_.visible()) {
            if (key.repeat == 0) inventory_overlay_.handle_key(symbol);
            return;
        }
        if (symbol == SDLK_BACKQUOTE) {
            set_console_visible(true);
            return;
        }
        if (key.repeat != 0 && symbol != SDLK_HOME) return;

        if (state_.state() == gl::core::AppState::level && level_ &&
            (symbol == SDLK_i || symbol == static_cast<SDL_Keycode>('I'))) {
            toggle_inventory();
            return;
        }

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
                mouse_down_ = false;
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
                mouse_down_ = false;
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
            rebuild_person_runtime({});
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
        const int last = static_cast<int>(display_saves_.size()) - 1;
        save_selected_ = std::clamp(save_selected_, 0, last);
        const gl::core::SaveSlotInfo& slot =
            *display_saves_[static_cast<std::size_t>(save_selected_)];
        load_equip_save(slot.slot);
        if (level_) {
            rebuild_person_runtime(level_->level().persons);
            apply_loaded_equip_save();
        }
    }

    void render(std::uint64_t now_ms, std::uint64_t animation_ms) {
        update_viewport();
        update_dialog_style();

        bool scene_drawn = false;
        if (state_.state() == gl::core::AppState::level && level_) {
            gl::app::LevelScreenOptions options;
            options.debug_overlay = debug_overlay_;
            options.walkable_overlay = options_.walkable_overlay;
            options.draw_persons = !options_.no_persons;
            options.draw_hero_wear = true;
            level_->render(window_.renderer(), options, animation_ms);
            scene_drawn = true;
            if (client_.snapshot().paused && !dialog_overlay_.active()) {
                const std::string_view label = "PAUSED";
                const float width = gl::render::text_width(label, kTitleScale);
                gl::render::draw_text(window_.renderer(), label,
                                      (static_cast<float>(viewport_width_) - width) * 0.5f,
                                      24.0f, kTitleScale,
                                      gl::render::Color{255, 210, 90, 255});
            }
        }

        if (dialog_overlay_.active()) {
            if (!scene_drawn) {
                SDL_SetRenderDrawBlendMode(window_.renderer(), SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(window_.renderer(), 12, 14, 20, 255);
                SDL_RenderClear(window_.renderer());
            }
            dialog_overlay_.render(window_.renderer(), font_ ? &*font_ : nullptr);
        } else if (!scene_drawn) {
            if (save_slots_) {
                render_save_slots();
            } else {
                render_menu();
            }
        }

        if (inventory_overlay_.visible()) {
            inventory_overlay_.render(window_.renderer(), viewport_width_, viewport_height_,
                                      inventory_font());
        }

        if (trade_overlay_.visible()) {
            trade_overlay_.render(window_.renderer(), viewport_width_, viewport_height_,
                                  inventory_font());
        }

        if (console_visible_) render_console(now_ms);
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
    std::optional<gl::assets::HeroEquipment> pending_equipment_;
    std::optional<gl::assets::HeroInventory> pending_inventory_;
    std::optional<gl::assets::HeroStats> pending_hero_stats_;
    std::optional<gl::game::HeroOutfit> pending_outfit_;
    gl::game::PersonRuntime person_runtime_;
    gl::game::InventoryRuntime inventory_runtime_;
    std::optional<gl::assets::ItemNameIndex> item_names_;
    bool hero_stats_absent_logged_ = false;
    gl::assets::ItemCatalog item_catalog_;
    std::optional<gl::assets::SpecialDatabase> specials_;
    std::string level_name_;
    std::string input_;
    bool console_visible_ = false;
    bool screenshot_mode_ = false;
    bool dirty_ = false;
    bool mouse_down_ = false;
    bool dragging_ = false;
    bool debug_overlay_ = false;
    SDL_FingerID finger_id_ = -1;
    bool finger_dragged_ = false;
    int finger_start_x_ = 0;
    int finger_start_y_ = 0;
    int finger_last_x_ = 0;
    int finger_last_y_ = 0;
    float finger_scroll_accum_ = 0.0f;
    int mouse_down_x_ = 0;
    int mouse_down_y_ = 0;
    int viewport_width_ = 0;
    int viewport_height_ = 0;
    int menu_selected_ = 0;
    int save_selected_ = 0;
    int save_scroll_ = 0;
    bool save_slots_ = false;
    std::string save_message_;
    std::optional<gl::render::Font> font_;
    std::optional<gl::render::Font> inventory_font_;
    std::unique_ptr<DialogContext> dialog_;
    gl::app::DialogOverlay dialog_overlay_;
    gl::app::InventoryOverlay inventory_overlay_;
    gl::app::TradeOverlay trade_overlay_;
    std::string current_trade_npc_;
    bool trade_hooks_run_ = false;
    std::optional<std::string> pending_trade_npc_;
    std::optional<std::size_t> pending_talk_;
    std::optional<gl::game::Tile> pending_talk_tile_;
    std::uint64_t level_animation_ms_ = 0;
    bool test_dialog_available_ = false;
    bool dialog_launched_from_cli_ = false;
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
