#include <gl/assets/log.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/game/level.hpp>
#include <gl/platform/sdl_context.hpp>
#include <gl/render/camera.hpp>
#include <gl/render/texture.hpp>

#include <SDL.h>
#include <stb_image_write.h>

#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

struct Options {
    std::filesystem::path game_dir;
    std::string level = "l1_1";
    std::filesystem::path screenshot;
    int width = 1280;
    int height = 720;
    int frames = 1;
    bool background_only = false;
};

struct Sprite {
    gl::render::Texture texture;
    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct Animation {
    std::vector<gl::render::Texture> frames;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t delay_ms = 0;
};

struct LevelGraphics {
    std::vector<Sprite> statics;
    std::vector<Animation> animations;
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

constexpr float kPanSpeed = 600.0f;
constexpr float kMinZoom = 0.25f;
constexpr float kMaxZoom = 4.0f;
constexpr float kZoomStep = 1.1f;

std::size_t animation_frame_index(const Animation& animation, std::uint64_t elapsed_ms) {
    if (animation.frames.empty() || animation.delay_ms == 0) return 0;
    return static_cast<std::size_t>((elapsed_ms / animation.delay_ms) %
                                    animation.frames.size());
}

bool rect_visible(const SDL_FRect& rect, int width, int height) {
    return rect.x + rect.w > 0.0f && rect.y + rect.h > 0.0f &&
           rect.x < static_cast<float>(width) && rect.y < static_cast<float>(height);
}

void draw_debug_overlay(SDL_Renderer* renderer, const LevelGraphics& graphics,
                        const gl::render::Camera& camera, int width, int height,
                        std::uint64_t elapsed_ms) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    for (const auto& sprite : graphics.statics) {
        const SDL_FRect rect =
            gl::render::to_screen(camera, static_cast<float>(sprite.x),
                                  static_cast<float>(sprite.y),
                                  static_cast<float>(sprite.texture.width()),
                                  static_cast<float>(sprite.texture.height()));
        if (rect_visible(rect, width, height)) SDL_RenderDrawRectF(renderer, &rect);
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    for (const auto& animation : graphics.animations) {
        if (animation.frames.empty()) continue;
        const gl::render::Texture& frame =
            animation.frames[animation_frame_index(animation, elapsed_ms)];
        const SDL_FRect rect =
            gl::render::to_screen(camera, static_cast<float>(animation.x),
                                  static_cast<float>(animation.y),
                                  static_cast<float>(frame.width()),
                                  static_cast<float>(frame.height()));
        if (rect_visible(rect, width, height)) SDL_RenderDrawRectF(renderer, &rect);
    }
}

void draw_frame(SDL_Renderer* renderer, const gl::render::Texture& background,
                const LevelGraphics& graphics, const gl::render::Camera& camera, int width,
                int height, std::uint64_t elapsed_ms, bool background_only,
                bool debug_overlay) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    gl::render::draw_background(renderer, background, camera, width, height);
    if (background_only) return;

    for (const auto& sprite : graphics.statics) {
        gl::render::draw_texture(renderer, sprite.texture, camera,
                                 static_cast<float>(sprite.x), static_cast<float>(sprite.y),
                                 width, height);
    }

    for (const auto& animation : graphics.animations) {
        if (animation.frames.empty()) continue;
        const std::size_t index = animation_frame_index(animation, elapsed_ms);
        gl::render::draw_texture(renderer, animation.frames[index], camera,
                                 static_cast<float>(animation.x),
                                 static_cast<float>(animation.y), width, height);
    }

    if (debug_overlay) {
        draw_debug_overlay(renderer, graphics, camera, width, height, elapsed_ms);
    }
}

int save_screenshot(SDL_Renderer* renderer, const std::filesystem::path& path, int frames,
                    const gl::render::Texture& background, const LevelGraphics& graphics,
                    const gl::render::Camera& camera, bool background_only) {
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
        const std::uint64_t elapsed_ms = static_cast<std::uint64_t>(frame + 1) * 16;
        draw_frame(renderer, background, graphics, camera, width, height, elapsed_ms,
                   background_only, false);
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

int run_interactive(SDL_Renderer* renderer, const gl::render::Texture& background,
                    const LevelGraphics& graphics, gl::render::Camera camera,
                    float world_width, float world_height, bool background_only) {
    const std::uint64_t start_ticks = SDL_GetTicks64();
    std::uint64_t previous_counter = SDL_GetPerformanceCounter();
    const double counter_frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    bool running = true;
    bool dragging = false;
    bool debug_overlay = false;

    while (running) {
        int width = 0;
        int height = 0;
        if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 ||
            height <= 0) {
            std::fprintf(stderr, "glview: cannot query renderer output size: %s\n",
                         SDL_GetError());
            return 2;
        }
        gl::render::clamp_camera(camera, world_width, world_height, width, height);

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
                    camera = gl::render::Camera{};
                    gl::render::clamp_camera(camera, world_width, world_height, width, height);
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                dragging = true;
            } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                dragging = false;
            } else if (event.type == SDL_MOUSEMOTION && dragging) {
                camera.x -= static_cast<float>(event.motion.xrel) / camera.zoom;
                camera.y -= static_cast<float>(event.motion.yrel) / camera.zoom;
                gl::render::clamp_camera(camera, world_width, world_height, width, height);
            } else if (event.type == SDL_MOUSEWHEEL && event.wheel.y != 0) {
                int mouse_x = 0;
                int mouse_y = 0;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                const float factor = event.wheel.y > 0 ? kZoomStep : 1.0f / kZoomStep;
                gl::render::zoom_camera_at(camera, static_cast<float>(mouse_x),
                                           static_cast<float>(mouse_y), factor, world_width,
                                           world_height, width, height, kMinZoom, kMaxZoom);
            } else if (event.type == SDL_WINDOWEVENT &&
                       (event.window.event == SDL_WINDOWEVENT_LEAVE ||
                        event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
                dragging = false;
            }
        }

        float move_x = 0.0f;
        float move_y = 0.0f;
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) move_x -= 1.0f;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) move_x += 1.0f;
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) move_y -= 1.0f;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) move_y += 1.0f;
        if (move_x != 0.0f || move_y != 0.0f) {
            const float length = std::sqrt(move_x * move_x + move_y * move_y);
            const float step = kPanSpeed / camera.zoom * delta;
            camera.x += move_x / length * step;
            camera.y += move_y / length * step;
            gl::render::clamp_camera(camera, world_width, world_height, width, height);
        }

        draw_frame(renderer, background, graphics, camera, width, height,
                   SDL_GetTicks64() - start_ticks, background_only, debug_overlay);
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

        auto level = gl::game::load_level(vfs.value(), options.level);
        if (!level) {
            report_error("cannot load level", level.error());
            return exit_code_for(level.error());
        }

        gl::log(gl::LogLevel::info,
                "level '" + level->name + "' (pack '" + level->pack + "'): " +
                    std::to_string(level->width) + "x" + std::to_string(level->height) +
                    " px, " + std::to_string(level->statics.size()) + " statics, " +
                    std::to_string(level->animations.size()) + " animations");

        gl::platform::SdlContext context;
        gl::platform::Window window("Goldenland 2 - Level Viewer", options.width, options.height);

        auto background = gl::render::Texture::from_image(window.renderer(), level->background);
        if (!background) {
            report_error("cannot create background texture", background.error());
            return exit_code_for(background.error());
        }

        LevelGraphics graphics;
        graphics.statics.reserve(level->statics.size());
        for (const auto& object : level->statics) {
            auto created = gl::render::Texture::from_image(window.renderer(), object.image);
            if (!created) {
                gl::log(gl::LogLevel::warning,
                        "level '" + level->name + "': skipping static " +
                            std::to_string(object.number) + " at (" +
                            std::to_string(object.x) + ", " + std::to_string(object.y) +
                            "): " + created.error().message);
                continue;
            }
            graphics.statics.push_back(
                Sprite{std::move(created.value()), object.x, object.y});
        }

        graphics.animations.reserve(level->animations.size());
        for (const auto& object : level->animations) {
            std::vector<gl::render::Texture> frames;
            frames.reserve(object.frames.size());
            bool complete = true;
            for (const auto& image : object.frames) {
                auto created = gl::render::Texture::from_image(window.renderer(), image);
                if (!created) {
                    gl::log(gl::LogLevel::warning,
                            "level '" + level->name + "': skipping animation " +
                                std::to_string(object.number) + " at (" +
                                std::to_string(object.x) + ", " + std::to_string(object.y) +
                                "): " + created.error().message);
                    complete = false;
                    break;
                }
                frames.push_back(std::move(created.value()));
            }
            if (!complete || frames.empty()) continue;
            graphics.animations.push_back(
                Animation{std::move(frames), object.x, object.y, object.delay_ms});
        }

        const gl::render::Camera camera;
        if (!options.screenshot.empty()) {
            return save_screenshot(window.renderer(), options.screenshot, options.frames,
                                   background.value(), graphics, camera,
                                   options.background_only);
        }
        return run_interactive(window.renderer(), background.value(), graphics, camera,
                               static_cast<float>(level->width),
                               static_cast<float>(level->height), options.background_only);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glview: %s\n", e.what());
        return 2;
    }
}
