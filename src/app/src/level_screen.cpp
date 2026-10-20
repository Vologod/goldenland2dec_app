#include "gl/app/level_screen.hpp"

#include <gl/assets/log.hpp>
#include <gl/render/animation.hpp>

#include <SDL.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace gl::app {
namespace {

constexpr float kMinZoom = 0.25f;
constexpr float kMaxZoom = 4.0f;

bool rect_visible(const SDL_FRect& rect, int width, int height) {
    return rect.x + rect.w > 0.0f && rect.y + rect.h > 0.0f &&
           rect.x < static_cast<float>(width) && rect.y < static_cast<float>(height);
}

}

LevelScreen::LevelScreen(gl::game::Level level, gl::render::Texture background)
    : level_(std::move(level)), background_(std::move(background)) {}

float LevelScreen::world_width() const { return static_cast<float>(level_.width); }

float LevelScreen::world_height() const { return static_cast<float>(level_.height); }

Result<LevelScreen> LevelScreen::load(SDL_Renderer* renderer, gl::assets::GameVfs& vfs,
                                      std::string_view level_name) {
    auto level = gl::game::load_level(vfs, level_name);
    if (!level) {
        return Error{level.error().code, "cannot load level: " + level.error().message};
    }

    gl::log(gl::LogLevel::info,
            "level '" + level->name + "' (pack '" + level->pack + "'): " +
                std::to_string(level->width) + "x" + std::to_string(level->height) + " px, " +
                std::to_string(level->statics.size()) + " statics, " +
                std::to_string(level->animations.size()) + " animations");

    auto background = gl::render::Texture::from_image(renderer, level->background);
    if (!background) {
        return Error{background.error().code,
                     "cannot create background texture: " + background.error().message};
    }

    LevelScreen screen(std::move(level.value()), std::move(background.value()));

    screen.statics_.reserve(screen.level_.statics.size());
    for (const auto& object : screen.level_.statics) {
        auto created = gl::render::Texture::from_image(renderer, object.image);
        if (!created) {
            gl::log(gl::LogLevel::warning,
                    "level '" + screen.level_.name + "': skipping static " +
                        std::to_string(object.number) + " at (" + std::to_string(object.x) +
                        ", " + std::to_string(object.y) + "): " + created.error().message);
            continue;
        }
        screen.statics_.push_back(Sprite{std::move(created.value()), object.x, object.y});
    }

    screen.animations_.reserve(screen.level_.animations.size());
    for (const auto& object : screen.level_.animations) {
        std::vector<gl::render::Texture> frames;
        frames.reserve(object.frames.size());
        bool complete = true;
        for (const auto& image : object.frames) {
            auto created = gl::render::Texture::from_image(renderer, image);
            if (!created) {
                gl::log(gl::LogLevel::warning,
                        "level '" + screen.level_.name + "': skipping animation " +
                            std::to_string(object.number) + " at (" + std::to_string(object.x) +
                            ", " + std::to_string(object.y) + "): " + created.error().message);
                complete = false;
                break;
            }
            frames.push_back(std::move(created.value()));
        }
        if (!complete || frames.empty()) continue;
        screen.animations_.push_back(
            Animation{std::move(frames), object.x, object.y, object.delay_ms});
    }

    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) == 0) {
        screen.screen_w_ = width;
        screen.screen_h_ = height;
    }

    return screen;
}

void LevelScreen::render(SDL_Renderer* renderer, const LevelScreenOptions& options,
                         std::uint64_t elapsed_ms, int screen_w, int screen_h) {
    screen_w_ = screen_w;
    screen_h_ = screen_h;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    gl::render::draw_background(renderer, background_, camera_, screen_w, screen_h);
    if (options.background_only) return;

    for (const auto& sprite : statics_) {
        gl::render::draw_texture(renderer, sprite.texture, camera_,
                                 static_cast<float>(sprite.x), static_cast<float>(sprite.y),
                                 screen_w, screen_h);
    }

    for (const auto& animation : animations_) {
        if (animation.frames.empty()) continue;
        const std::size_t index = gl::render::animation_frame_index(
            elapsed_ms, animation.delay_ms, animation.frames.size());
        gl::render::draw_texture(renderer, animation.frames[index], camera_,
                                 static_cast<float>(animation.x),
                                 static_cast<float>(animation.y), screen_w, screen_h);
    }

    if (options.debug_overlay) {
        draw_debug_overlay(renderer, screen_w, screen_h, elapsed_ms);
    }
}

void LevelScreen::draw_debug_overlay(SDL_Renderer* renderer, int screen_w, int screen_h,
                                     std::uint64_t elapsed_ms) const {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    for (const auto& sprite : statics_) {
        const SDL_FRect rect =
            gl::render::to_screen(camera_, static_cast<float>(sprite.x),
                                  static_cast<float>(sprite.y),
                                  static_cast<float>(sprite.texture.width()),
                                  static_cast<float>(sprite.texture.height()));
        if (rect_visible(rect, screen_w, screen_h)) SDL_RenderDrawRectF(renderer, &rect);
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    for (const auto& animation : animations_) {
        if (animation.frames.empty()) continue;
        const gl::render::Texture& frame =
            animation.frames[gl::render::animation_frame_index(elapsed_ms, animation.delay_ms,
                                                               animation.frames.size())];
        const SDL_FRect rect =
            gl::render::to_screen(camera_, static_cast<float>(animation.x),
                                  static_cast<float>(animation.y),
                                  static_cast<float>(frame.width()),
                                  static_cast<float>(frame.height()));
        if (rect_visible(rect, screen_w, screen_h)) SDL_RenderDrawRectF(renderer, &rect);
    }
}

void LevelScreen::pan(float world_dx, float world_dy) {
    if (!std::isfinite(world_dx) || !std::isfinite(world_dy)) return;
    camera_.x += world_dx;
    camera_.y += world_dy;
    gl::render::clamp_camera(camera_, world_width(), world_height(), screen_w_, screen_h_);
}

void LevelScreen::zoom_at(float factor, float screen_x, float screen_y, int screen_w,
                          int screen_h) {
    screen_w_ = screen_w;
    screen_h_ = screen_h;
    gl::render::zoom_camera_at(camera_, screen_x, screen_y, factor, world_width(),
                               world_height(), screen_w_, screen_h_, kMinZoom, kMaxZoom);
}

void LevelScreen::reset_camera() {
    camera_ = gl::render::Camera{};
    gl::render::clamp_camera(camera_, world_width(), world_height(), screen_w_, screen_h_);
}

void LevelScreen::set_viewport(int screen_w, int screen_h) {
    screen_w_ = screen_w;
    screen_h_ = screen_h;
    gl::render::clamp_camera(camera_, world_width(), world_height(), screen_w_, screen_h_);
}

}
