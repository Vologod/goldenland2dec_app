#include <gl/app/level_screen.hpp>
#include <gl/assets/log.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/platform/sdl_context.hpp>

#include <SDL.h>
#include <stb_image_write.h>

#include <algorithm>
#include <charconv>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

constexpr std::uint64_t kMaxSimMs = 86'400'000;

struct Options {
    std::filesystem::path game_dir;
    std::string level = "l1_1";
    std::filesystem::path screenshot;
    int width = 1280;
    int height = 720;
    int frames = 1;
    bool background_only = false;
    bool debug_overlay = false;
    bool walkable_overlay = false;
    bool no_persons = false;
    bool no_masks = false;
    std::uint64_t sim_ms = 0;
    bool sim_ms_set = false;
    std::optional<int> camera_x;
    std::optional<int> camera_y;
    struct PlacePerson {
        std::size_t index = 0;
        std::int32_t x = 0;
        std::int32_t y = 0;
    };
    std::optional<PlacePerson> place_person;
};

enum class ParseStatus { ok, help, error };

void print_usage() {
    std::puts("Usage: glview --game-dir <dir> [options]\n"
              "\n"
              "Options:\n"
              "  --game-dir <dir>   Game installation directory (required)\n"
              "  --level <name>     Level name (default: l1_1)\n"
              "  --width <px>       Window width (default: 1280)\n"
              "  --height <px>      Window height (default: 720)\n"
              "  --screenshot <png> Render frames, save PNG and exit\n"
              "  --frames <n>       Frames to render before screenshot (default: 1)\n"
              "  --background-only  Draw only the background (skip statics and animations)\n"
              "  --no-persons       Skip NPC sprites\n"
              "  --sim-ms <ms>      Simulation time for screenshots (ignored interactively)\n"
              "  --camera-x <px>    World X of the left viewport edge (screenshot mode)\n"
              "  --camera-y <px>    World Y of the top viewport edge (screenshot mode)\n"
              "  --place-person <index> <tile_x> <tile_y>\n"
              "                     Teleport a person before the screenshot\n"
              "  --no-masks         Skip foreground mask overlays\n"
              "  --debug-overlay    Draw sprite debug rectangles in screenshot mode\n"
              "  --walkable-overlay Draw blocked tiles in red on the level\n"
              "  -h, --help         Show this help and exit\n"
              "\n"
              "Exit codes: 0 success, 1 arguments, 2 I/O, 3 format/load.");
}

std::filesystem::path path_from_utf8(std::string_view text) {
    std::u8string utf8;
    utf8.reserve(text.size());
    for (const char c : text) utf8.push_back(static_cast<char8_t>(c));
    return std::filesystem::path(utf8);
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
    return parse_int(text, value) && value > 0 && value <= 16384;
}

bool parse_frames(std::string_view text, int& value) {
    return parse_int(text, value) && value > 0 && value <= 1000000;
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
                std::fprintf(stderr, "glview: missing value for %.*s\n",
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
                std::fputs("glview: --game-dir must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.game_dir = path_from_utf8(value);
            has_game_dir = true;
        } else if (arg == "--level") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value.empty()) {
                std::fputs("glview: --level must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.level = std::string(value);
        } else if (arg == "--width") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (!parse_dimension(value, options.width)) {
                std::fprintf(stderr, "glview: invalid --width: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
        } else if (arg == "--height") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (!parse_dimension(value, options.height)) {
                std::fprintf(stderr, "glview: invalid --height: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
        } else if (arg == "--screenshot") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (value.empty()) {
                std::fputs("glview: --screenshot must not be empty\n", stderr);
                return ParseStatus::error;
            }
            options.screenshot = path_from_utf8(value);
        } else if (arg == "--frames") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (!parse_frames(value, options.frames)) {
                std::fprintf(stderr, "glview: invalid --frames: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
        } else if (arg == "--background-only") {
            options.background_only = true;
        } else if (arg == "--no-persons") {
            options.no_persons = true;
        } else if (arg == "--sim-ms") {
            if (!next_value(arg, value)) return ParseStatus::error;
            if (!parse_milliseconds(value, options.sim_ms)) {
                std::fprintf(stderr, "glview: invalid --sim-ms: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.sim_ms_set = true;
        } else if (arg == "--camera-x") {
            if (!next_value(arg, value)) return ParseStatus::error;
            int parsed = 0;
            if (!parse_int(value, parsed) || parsed < 0 || parsed > 1'000'000) {
                std::fprintf(stderr, "glview: invalid --camera-x: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.camera_x = parsed;
        } else if (arg == "--camera-y") {
            if (!next_value(arg, value)) return ParseStatus::error;
            int parsed = 0;
            if (!parse_int(value, parsed) || parsed < 0 || parsed > 1'000'000) {
                std::fprintf(stderr, "glview: invalid --camera-y: %.*s\n",
                             static_cast<int>(value.size()), value.data());
                return ParseStatus::error;
            }
            options.camera_y = parsed;
        } else if (arg == "--no-masks") {
            options.no_masks = true;
        } else if (arg == "--place-person") {
            if (i + 3 >= argc) {
                std::fputs("glview: --place-person needs <index> <tile_x> <tile_y>\n", stderr);
                return ParseStatus::error;
            }
            int index = 0;
            int tile_x = 0;
            int tile_y = 0;
            if (!parse_int(argv[i + 1], index) || index < 0 ||
                !parse_int(argv[i + 2], tile_x) || tile_x < 0 ||
                !parse_int(argv[i + 3], tile_y) || tile_y < 0) {
                std::fputs("glview: invalid --place-person arguments\n", stderr);
                return ParseStatus::error;
            }
            options.place_person =
                Options::PlacePerson{static_cast<std::size_t>(index), tile_x, tile_y};
            i += 3;
        } else if (arg == "--debug-overlay") {
            options.debug_overlay = true;
        } else if (arg == "--walkable-overlay") {
            options.walkable_overlay = true;
        } else {
            std::fprintf(stderr, "glview: unknown argument: %s\n", argv[i]);
            print_usage();
            return ParseStatus::error;
        }
    }

    if (!has_game_dir) {
        std::fputs("glview: --game-dir is required\n", stderr);
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

void report_error(std::string_view context, const gl::Error& error) {
    std::fprintf(stderr, "glview: %.*s: %s\n", static_cast<int>(context.size()), context.data(),
                 error.message.c_str());
}

constexpr float kZoomStep = 1.1f;

int save_screenshot(SDL_Renderer* renderer, const std::filesystem::path& path, int frames,
                    std::uint64_t sim_ms, bool sim_ms_set, gl::app::LevelScreen& screen,
                    const gl::app::LevelScreenOptions& screen_options) {
    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) {
        std::fprintf(stderr, "glview: cannot query renderer output size: %s\n", SDL_GetError());
        return 2;
    }

    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) *
                                      static_cast<std::uint64_t>(height);
    if (pixel_count > static_cast<std::uint64_t>(INT_MAX) || width > INT_MAX / 4) {
        std::fprintf(stderr, "glview: renderer output too large: %dx%d\n", width, height);
        return 3;
    }
    const int pitch = width * 4;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(pixel_count) * 4);

    for (int frame = 0; frame < frames; ++frame) {
        const std::uint64_t elapsed_ms =
            sim_ms_set ? sim_ms : static_cast<std::uint64_t>(frame + 1) * 16;
        screen.render(renderer, screen_options, elapsed_ms);
        if (frame + 1 == frames) {
            if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels.data(),
                                     pitch) != 0) {
                std::fprintf(stderr, "glview: SDL_RenderReadPixels failed: %s\n", SDL_GetError());
                return 2;
            }
        }
        SDL_RenderPresent(renderer);
    }

    if (stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), pitch) == 0) {
        std::fprintf(stderr, "glview: cannot write screenshot: %s\n", path.string().c_str());
        return 2;
    }

    gl::log(gl::LogLevel::info,
            "saved screenshot " + path.string() + " (" + std::to_string(width) + "x" +
                std::to_string(height) + ")");
    return 0;
}

int run_interactive(SDL_Renderer* renderer, gl::app::LevelScreen& screen,
                    const gl::app::LevelScreenOptions& options, int initial_width,
                    int initial_height) {
    const std::uint64_t start_ticks = SDL_GetTicks64();
    std::uint64_t previous_counter = SDL_GetPerformanceCounter();
    const double counter_frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    bool running = true;
    bool dragging = false;
    bool debug_overlay = false;
    int last_width = initial_width;
    int last_height = initial_height;

    while (running) {
        int width = 0;
        int height = 0;
        if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 ||
            height <= 0) {
            std::fprintf(stderr, "glview: cannot query renderer output size: %s\n",
                         SDL_GetError());
            return 2;
        }
        if (width != last_width || height != last_height) {
            screen.set_viewport(width, height);
            last_width = width;
            last_height = height;
        }

        const std::uint64_t now_counter = SDL_GetPerformanceCounter();
        double delta_seconds =
            static_cast<double>(now_counter - previous_counter) / counter_frequency;
        previous_counter = now_counter;
        delta_seconds = std::min(delta_seconds, 0.1);
        const float delta = static_cast<float>(delta_seconds);

        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.repeat != 0) continue;
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_F1) {
                    debug_overlay = !debug_overlay;
                } else if (event.key.keysym.sym == SDLK_HOME) {
                    screen.reset_camera();
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                dragging = true;
            } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                dragging = false;
            } else if (event.type == SDL_MOUSEMOTION && dragging) {
                const float zoom = screen.camera().zoom;
                screen.pan(-static_cast<float>(event.motion.xrel) / zoom,
                           -static_cast<float>(event.motion.yrel) / zoom);
            } else if (event.type == SDL_MOUSEWHEEL && event.wheel.y != 0) {
                int mouse_x = 0;
                int mouse_y = 0;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                const float factor = event.wheel.y > 0 ? kZoomStep : 1.0f / kZoomStep;
                screen.zoom_at(factor, static_cast<float>(mouse_x),
                               static_cast<float>(mouse_y));
            } else if (event.type == SDL_WINDOWEVENT &&
                       (event.window.event == SDL_WINDOWEVENT_LEAVE ||
                        event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
                dragging = false;
            }
        }

        int move_x = 0;
        int move_y = 0;
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) move_x -= 1;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) move_x += 1;
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) move_y -= 1;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) move_y += 1;
        if (move_x != 0 || move_y != 0) {
            screen.pan_input(move_x, move_y, delta);
        }

        gl::app::LevelScreenOptions frame_options = options;
        frame_options.debug_overlay = debug_overlay;
        screen.render(renderer, frame_options, SDL_GetTicks64() - start_ticks);
        SDL_RenderPresent(renderer);
    }
    return 0;
}

}

int main(int argc, char** argv) {
    Options options;
    try {
        const ParseStatus status = parse_args(argc, argv, options);
        if (status == ParseStatus::help) return 0;
        if (status == ParseStatus::error) return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glview: %s\n", e.what());
        return 1;
    }

    try {
        auto vfs = gl::assets::GameVfs::mount(options.game_dir);
        if (!vfs) {
            report_error("cannot mount game directory", vfs.error());
            return exit_code_for(vfs.error());
        }

        gl::platform::SdlContext context;
        gl::platform::Window window("Goldenland 2 - Level Viewer", options.width, options.height);

        auto screen = gl::app::LevelScreen::load(window.renderer(), vfs.value(), options.level,
                                                 !options.no_persons);
        if (!screen) {
            std::fprintf(stderr, "glview: %s\n", screen.error().message.c_str());
            return exit_code_for(screen.error());
        }

        int width = 0;
        int height = 0;
        if (SDL_GetRendererOutputSize(window.renderer(), &width, &height) != 0 || width <= 0 ||
            height <= 0) {
            std::fprintf(stderr, "glview: cannot query renderer output size: %s\n",
                         SDL_GetError());
            return 2;
        }
        if (options.place_person) {
            screen.value().simulation().place_person(
                options.place_person->index,
                gl::game::Tile{options.place_person->x, options.place_person->y});
        }
        screen.value().set_viewport(width, height);
        if (options.camera_x && options.camera_y) {
            const gl::render::Camera& camera = screen.value().camera();
            screen.value().pan(static_cast<float>(*options.camera_x) - camera.x,
                               static_cast<float>(*options.camera_y) - camera.y);
        }

        gl::app::LevelScreenOptions screen_options;
        screen_options.background_only = options.background_only;
        screen_options.debug_overlay = options.debug_overlay;
        screen_options.walkable_overlay = options.walkable_overlay;
        screen_options.draw_persons = !options.no_persons;
        screen_options.draw_masks = !options.no_masks;
        if (!options.screenshot.empty()) {
            return save_screenshot(window.renderer(), options.screenshot, options.frames,
                                   options.sim_ms, options.sim_ms_set, screen.value(),
                                   screen_options);
        }
        return run_interactive(window.renderer(), screen.value(), screen_options, width, height);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glview: %s\n", e.what());
        return 2;
    }
}
