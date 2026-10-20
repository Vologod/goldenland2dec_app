#include <gl/assets/log.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/game/level.hpp>
#include <gl/platform/sdl_context.hpp>
#include <gl/render/camera.hpp>
#include <gl/render/texture.hpp>

#include <SDL.h>
#include <stb_image_write.h>

#include <charconv>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct Options {
    std::filesystem::path game_dir;
    std::string level = "l1_1";
    std::filesystem::path screenshot;
    int width = 1280;
    int height = 720;
    int frames = 1;
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

void draw_frame(SDL_Renderer* renderer, const gl::render::Texture& texture,
                const gl::render::Camera& camera, int width, int height) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    gl::render::draw_background(renderer, texture, camera, width, height);
}

int save_screenshot(SDL_Renderer* renderer, const std::filesystem::path& path, int frames,
                    const gl::render::Texture& texture, const gl::render::Camera& camera) {
    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) {
        std::fprintf(stderr, "glview: cannot query renderer output size: %s\n", SDL_GetError());
        return 2;
    }

    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) *
                                      static_cast<std::uint64_t>(height);
    if (pixel_count > static_cast<std::uint64_t>(INT_MAX) || width > INT_MAX / 4) return 3;
    const int pitch = width * 4;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(pixel_count) * 4);

    for (int frame = 0; frame < frames; ++frame) {
        draw_frame(renderer, texture, camera, width, height);
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

int run_interactive(SDL_Renderer* renderer, const gl::render::Texture& texture,
                    const gl::render::Camera& camera) {
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        int width = 0;
        int height = 0;
        if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 ||
            height <= 0) {
            std::fprintf(stderr, "glview: cannot query renderer output size: %s\n",
                         SDL_GetError());
            return 2;
        }

        draw_frame(renderer, texture, camera, width, height);
        SDL_RenderPresent(renderer);
    }
    return 0;
}

}

int main(int argc, char** argv) {
    Options options;
    const ParseStatus status = parse_args(argc, argv, options);
    if (status == ParseStatus::help) return 0;
    if (status == ParseStatus::error) return 1;

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

        gl::platform::SdlContext context;
        gl::platform::Window window("Goldenland 2 - Level Viewer", options.width, options.height);

        auto texture = gl::render::Texture::from_image(window.renderer(), level->background);
        if (!texture) {
            report_error("cannot create background texture", texture.error());
            return exit_code_for(texture.error());
        }

        const gl::render::Camera camera;
        if (!options.screenshot.empty()) {
            return save_screenshot(window.renderer(), options.screenshot, options.frames,
                                   texture.value(), camera);
        }
        return run_interactive(window.renderer(), texture.value(), camera);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glview: %s\n", e.what());
        return 2;
    }
}
