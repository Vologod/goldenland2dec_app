#include "gl/app/level_screen.hpp"

#include "gl/app/person_render.hpp"

#include <gl/assets/csx.hpp>
#include <gl/assets/image.hpp>
#include <gl/assets/log.hpp>
#include <gl/render/animation.hpp>

#include <SDL.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace gl::app {
namespace {

constexpr float kMinZoom = 0.25f;
constexpr float kMaxZoom = 4.0f;
constexpr float kPanSpeed = 600.0f;
constexpr float kTileWidth = 12.0f;
constexpr float kTileHeight = 9.0f;

bool rect_visible(const SDL_FRect& rect, int width, int height) {
    return rect.x + rect.w > 0.0f && rect.y + rect.h > 0.0f &&
           rect.x < static_cast<float>(width) && rect.y < static_cast<float>(height);
}

bool source_fits(const SDL_Rect& source, const gl::render::Texture& texture) {
    return source.w > 0 && source.h > 0 && source.x >= 0 && source.y >= 0 &&
           source.x + source.w <= texture.width() && source.y + source.h <= texture.height();
}

void draw_source(SDL_Renderer* renderer, const gl::render::Texture& texture,
                 const SDL_Rect& source, const SDL_FRect& destination,
                 SDL_RendererFlip flip = SDL_FLIP_NONE) {
    if (!source_fits(source, texture)) return;
    SDL_RenderCopyExF(renderer, texture.get(), &source, &destination, 0.0, nullptr, flip);
}

std::string lower_path(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        } else if (c == '\\') {
            c = '/';
        }
    }
    return out;
}

std::string person_directory(std::string_view res_name) {
    return "persons/" + lower_path(res_name);
}

bool decode_person_image(gl::assets::GameVfs& vfs, const std::string& path,
                         gl::assets::Image& out) {
    auto data = vfs.read(path);
    if (!data) return false;
    auto csx = gl::assets::decode_csx(data.value());
    if (!csx) return false;
    out.width = csx.value().width;
    out.height = csx.value().height;
    out.rgba = std::move(csx.value().rgba);
    return true;
}

}

LevelScreen::LevelScreen(gl::game::Level level, gl::render::Texture background)
    : level_(std::move(level)), background_(std::move(background)) {}

float LevelScreen::world_width() const { return static_cast<float>(level_.width); }

float LevelScreen::world_height() const { return static_cast<float>(level_.height); }

Result<LevelScreen> LevelScreen::load(SDL_Renderer* renderer, gl::assets::GameVfs& vfs,
                                      std::string_view level_name, bool load_persons) {
    auto level = gl::game::load_level(vfs, level_name);
    if (!level) {
        return Error{level.error().code, "cannot load level: " + level.error().message};
    }

    gl::log(gl::LogLevel::info,
            "level '" + level->name + "' (pack '" + level->pack + "'): " +
                std::to_string(level->width) + "x" + std::to_string(level->height) + " px, " +
                std::to_string(level->statics.size()) + " statics, " +
                std::to_string(level->animations.size()) + " animations, " +
                std::to_string(level->persons.size()) + " persons");

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

    if (load_persons) {
        screen.persons_.reserve(screen.level_.persons.size());
        for (std::size_t instance_index = 0; instance_index < screen.level_.persons.size();
             ++instance_index) {
            const gl::game::PersonInstance& instance = screen.level_.persons[instance_index];
            std::size_t animation_index = 0;
            bool found = false;
            for (std::size_t i = 0; i < instance.pad.size(); ++i) {
                if (instance.pad[i].type == instance.stay_type) {
                    animation_index = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                gl::log(gl::LogLevel::warning,
                        "level '" + screen.level_.name + "': skipping person '" +
                            instance.name +
                            "': PAD has no animation selected by the loader");
                continue;
            }

            const gl::assets::PadAnimation& animation = instance.pad[animation_index];
            if (animation.frames_per_row <= 0 || animation.frame_width <= 0 ||
                animation.frame_height <= 0 || animation.crops.empty()) {
                gl::log(gl::LogLevel::warning,
                        "level '" + screen.level_.name + "': skipping person '" +
                            instance.name + "': invalid standing animation geometry");
                continue;
            }

            const std::uint64_t expected_width =
                static_cast<std::uint64_t>(animation.frame_width) *
                static_cast<std::uint64_t>(animation.frames_per_row);
            const std::uint64_t expected_height =
                static_cast<std::uint64_t>(animation.frame_height) *
                static_cast<std::uint64_t>(animation.crops.size());
            if (static_cast<std::uint64_t>(instance.sprite.width) < expected_width ||
                static_cast<std::uint64_t>(instance.sprite.height) < expected_height) {
                gl::log(gl::LogLevel::warning,
                        "level '" + screen.level_.name + "': skipping person '" +
                            instance.name + "': sprite sheet " +
                            std::to_string(instance.sprite.width) + "x" +
                            std::to_string(instance.sprite.height) + " is smaller than " +
                            std::to_string(expected_width) + "x" +
                            std::to_string(expected_height));
                continue;
            }

            auto texture = gl::render::Texture::from_image(renderer, instance.sprite);
            if (!texture) {
                gl::log(gl::LogLevel::warning,
                        "level '" + screen.level_.name + "': skipping person '" +
                            instance.name + "': " + texture.error().message);
                continue;
            }

            std::optional<gl::render::Texture> shadow;
            const std::uint64_t shadow_width =
                static_cast<std::uint64_t>(animation.shadow_width) *
                static_cast<std::uint64_t>(animation.frames_per_row);
            const std::uint64_t shadow_height =
                static_cast<std::uint64_t>(animation.shadow_height) *
                static_cast<std::uint64_t>(animation.shadow_crops.size());
            if (animation.shadow_width > 0 && animation.shadow_height > 0 &&
                static_cast<std::uint64_t>(instance.shadow.width) >= shadow_width &&
                static_cast<std::uint64_t>(instance.shadow.height) >= shadow_height) {
                auto created_shadow =
                    gl::render::Texture::from_image(renderer, instance.shadow);
                if (created_shadow) {
                    shadow.emplace(std::move(created_shadow.value()));
                } else {
                    gl::log(gl::LogLevel::warning,
                            "level '" + screen.level_.name + "': person '" + instance.name +
                                "' has no usable shadow: " + created_shadow.error().message);
                }
            }

            std::size_t go_animation = 0;
            std::optional<gl::render::Texture> go_texture;
            std::optional<gl::render::Texture> go_shadow;
            if (instance.go_type) {
                const std::string directory = person_directory(instance.res_name);
                const std::string go_sheet =
                    *instance.go_type == gl::assets::PadAnimationType::rt_go ? "rt_go"
                                                                            : "tb_go";
                bool found_go = false;
                for (std::size_t i = 0; i < instance.pad.size(); ++i) {
                    if (instance.pad[i].type == *instance.go_type) {
                        go_animation = i;
                        found_go = true;
                        break;
                    }
                }
                if (!found_go) {
                    gl::log(gl::LogLevel::warning,
                            "level '" + screen.level_.name + "': person '" + instance.name +
                                "': PAD has no selected go animation");
                } else {
                    const gl::assets::PadAnimation& go = instance.pad[go_animation];
                    const std::string animation_path =
                        directory + "/animation/" + go_sheet + ".csx";
                    gl::assets::Image go_image;
                    if (go.frames_per_row <= 0 || go.frame_width <= 0 ||
                        go.frame_height <= 0 || go.crops.empty()) {
                        gl::log(gl::LogLevel::warning,
                                "level '" + screen.level_.name + "': person '" +
                                    instance.name + "': invalid go animation geometry");
                    } else if (!decode_person_image(vfs, animation_path, go_image)) {
                        gl::log(gl::LogLevel::warning,
                                "level '" + screen.level_.name + "': person '" +
                                    instance.name + "': cannot read go sheet '" +
                                    animation_path + "'");
                    } else {
                        const std::uint64_t go_width =
                            static_cast<std::uint64_t>(go.frame_width) *
                            static_cast<std::uint64_t>(go.frames_per_row);
                        const std::uint64_t go_height =
                            static_cast<std::uint64_t>(go.frame_height) *
                            static_cast<std::uint64_t>(go.crops.size());
                        if (static_cast<std::uint64_t>(go_image.width) < go_width ||
                            static_cast<std::uint64_t>(go_image.height) < go_height) {
                            gl::log(gl::LogLevel::warning,
                                    "level '" + screen.level_.name + "': person '" +
                                        instance.name + "': go sheet is smaller than " +
                                        std::to_string(go_width) + "x" +
                                        std::to_string(go_height));
                        } else {
                            auto created =
                                gl::render::Texture::from_image(renderer, go_image);
                            if (created) {
                                go_texture.emplace(std::move(created.value()));
                            } else {
                                gl::log(gl::LogLevel::warning,
                                        "level '" + screen.level_.name + "': person '" +
                                            instance.name +
                                            "': cannot create go texture: " +
                                            created.error().message);
                            }
                        }
                    }
                    if (go_texture && go.shadow_width > 0 && go.shadow_height > 0 &&
                        !go.shadow_crops.empty()) {
                        const std::string shadow_path =
                            directory + "/shadows/" + go_sheet + ".csx";
                        gl::assets::Image go_shadow_image;
                        if (decode_person_image(vfs, shadow_path, go_shadow_image)) {
                            const std::uint64_t go_shadow_width =
                                static_cast<std::uint64_t>(go.shadow_width) *
                                static_cast<std::uint64_t>(go.frames_per_row);
                            const std::uint64_t go_shadow_height =
                                static_cast<std::uint64_t>(go.shadow_height) *
                                static_cast<std::uint64_t>(go.shadow_crops.size());
                            if (static_cast<std::uint64_t>(go_shadow_image.width) >=
                                    go_shadow_width &&
                                static_cast<std::uint64_t>(go_shadow_image.height) >=
                                    go_shadow_height) {
                                auto created_shadow = gl::render::Texture::from_image(
                                    renderer, go_shadow_image);
                                if (created_shadow) {
                                    go_shadow.emplace(
                                        std::move(created_shadow.value()));
                                }
                            }
                        }
                    }
                }
            }

            screen.persons_.push_back(Person{std::move(texture.value()), std::move(shadow),
                                             std::move(go_texture), std::move(go_shadow),
                                             instance_index, animation_index, go_animation});
        }

        screen.simulation_ = gl::game::LevelSimulation::create(screen.level_, 1);
    }

    if (!screen.persons_.empty()) {
        gl::log(gl::LogLevel::info,
                "level '" + screen.level_.name + "': uploaded " +
                    std::to_string(screen.persons_.size()) + " person sprites");
    }

    return screen;
}

void LevelScreen::render(SDL_Renderer* renderer, const LevelScreenOptions& options,
                         std::uint64_t elapsed_ms) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    gl::render::draw_background(renderer, background_, camera_, screen_w_, screen_h_);
    if (options.background_only) return;

    for (const auto& sprite : statics_) {
        gl::render::draw_texture(renderer, sprite.texture, camera_,
                                 static_cast<float>(sprite.x), static_cast<float>(sprite.y),
                                 screen_w_, screen_h_);
    }

    for (const auto& animation : animations_) {
        if (animation.frames.empty()) continue;
        const std::size_t index = gl::render::animation_frame_index(
            elapsed_ms, animation.delay_ms, animation.frames.size());
        gl::render::draw_texture(renderer, animation.frames[index], camera_,
                                 static_cast<float>(animation.x),
                                 static_cast<float>(animation.y), screen_w_, screen_h_);
    }

    if (options.draw_persons) {
        if (!persons_.empty()) {
            const std::uint64_t target = elapsed_ms;
            if (target > last_simulation_ms_) {
                simulation_.advance(
                    static_cast<double>(target - last_simulation_ms_) / 1000.0);
                last_simulation_ms_ = target;
            }
        }
        simulated_ms_ = simulation_.time_ms();
        for (const Person& person : persons_) {
            draw_person(renderer, person, simulated_ms_);
        }
    }

    if (options.walkable_overlay) {
        draw_walkable_overlay(renderer);
    }

    if (options.debug_overlay) {
        draw_debug_overlay(renderer, screen_w_, screen_h_, elapsed_ms);
    }
}

void LevelScreen::draw_person(SDL_Renderer* renderer, const Person& person,
                              std::uint64_t elapsed_ms) const {
    const gl::game::PersonInstance& instance = level_.persons[person.instance];
    if (person.instance >= simulation_.persons().size()) return;
    const gl::game::PersonMovement& movement = simulation_.persons()[person.instance];
    const bool moving = movement.moving && person.go_texture.has_value() &&
                        person.go_animation < instance.pad.size();
    const gl::assets::PadAnimation& animation =
        moving ? instance.pad[person.go_animation] : instance.pad[person.animation];
    const gl::render::Texture& texture = moving ? *person.go_texture : person.texture;
    const std::uint8_t direction = moving ? movement.direction16 : movement.direction8;
    const std::uint64_t frame_elapsed =
        elapsed_ms > movement.state_started_ms ? elapsed_ms - movement.state_started_ms : 0;

    const PersonPlacement placement =
        place_person_px(animation, direction, movement.x, movement.y);
    if (placement.row >= animation.crops.size()) return;

    const std::size_t column =
        person_frame_index(frame_elapsed, animation.delay, animation.frames_per_row);

    const std::optional<gl::render::Texture>& shadow_texture =
        moving ? person.go_shadow : person.shadow;
    if (shadow_texture) {
        const PersonShadowPlacement shadow =
            place_person_shadow_px(animation, direction, movement.x, movement.y);
        if (shadow.row < animation.shadow_crops.size()) {
            const SDL_Rect source{
                static_cast<int>(column) * animation.shadow_width,
                static_cast<int>(shadow.row) * animation.shadow_height,
                animation.shadow_width,
                animation.shadow_height,
            };
            const SDL_FRect destination =
                gl::render::to_screen(camera_, shadow.draw_x, shadow.draw_y,
                                      static_cast<float>(animation.shadow_width),
                                      static_cast<float>(animation.shadow_height));
            if (rect_visible(destination, screen_w_, screen_h_)) {
                draw_source(renderer, *shadow_texture, source, destination);
            }
        }
    }

    const SDL_Rect source{
        static_cast<int>(column) * animation.frame_width,
        static_cast<int>(placement.row) * animation.frame_height,
        animation.frame_width,
        animation.frame_height,
    };
    const SDL_FRect destination =
        gl::render::to_screen(camera_, placement.draw_x, placement.draw_y,
                              static_cast<float>(animation.frame_width),
                              static_cast<float>(animation.frame_height));
    if (!rect_visible(destination, screen_w_, screen_h_)) return;

    draw_source(renderer, texture, source, destination,
                placement.mirror ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
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

void LevelScreen::draw_walkable_overlay(SDL_Renderer* renderer) const {
    if (level_.walkable.empty()) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 96);
    for (std::int32_t y = 0; y < level_.walkable.height; ++y) {
        for (std::int32_t x = 0; x < level_.walkable.width; ++x) {
            if (level_.walkable.is_walkable(x, y)) continue;
            const SDL_FRect rect = gl::render::to_screen(
                camera_, static_cast<float>(x) * kTileWidth,
                static_cast<float>(y) * kTileHeight, kTileWidth, kTileHeight);
            if (rect_visible(rect, screen_w_, screen_h_)) SDL_RenderFillRectF(renderer, &rect);
        }
    }
}

void LevelScreen::pan(float world_dx, float world_dy) {
    if (!std::isfinite(world_dx) || !std::isfinite(world_dy)) return;
    camera_.x += world_dx;
    camera_.y += world_dy;
    gl::render::clamp_camera(camera_, world_width(), world_height(), screen_w_, screen_h_);
}

void LevelScreen::pan_input(int x_axis, int y_axis, float delta_seconds) {
    if (x_axis == 0 && y_axis == 0) return;
    gl::render::pan_camera(camera_, static_cast<float>(x_axis), static_cast<float>(y_axis),
                           kPanSpeed, delta_seconds, world_width(), world_height(), screen_w_,
                           screen_h_);
}

void LevelScreen::zoom_at(float factor, float screen_x, float screen_y) {
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
