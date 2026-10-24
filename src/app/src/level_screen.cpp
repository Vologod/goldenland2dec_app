#include "gl/app/level_screen.hpp"

#include "gl/app/mask_render.hpp"
#include "gl/app/person_render.hpp"

#include <gl/assets/csx.hpp>
#include <gl/assets/image.hpp>
#include <gl/assets/log.hpp>
#include <gl/assets/pad.hpp>
#include <gl/game/person_movement.hpp>
#include <gl/render/animation.hpp>

#include <SDL.h>

#include <algorithm>
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

gl::game::Tile screen_to_tile(const gl::render::Camera& camera, int sx, int sy) {
    const float zoom = camera.zoom > 0.0f ? camera.zoom : 1.0f;
    return gl::game::tile_at(camera.x + static_cast<float>(sx) / zoom,
                             camera.y + static_cast<float>(sy) / zoom);
}

bool world_to_tile(const gl::render::Camera& camera, const gl::game::WalkGrid& grid,
                   int sx, int sy, gl::game::Tile& out) {
    out = screen_to_tile(camera, sx, sy);
    if (grid.empty()) return true;
    return out.x >= 0 && out.y >= 0 && out.x < grid.width && out.y < grid.height;
}

PersonHitRect person_hit_rect(const gl::game::PersonInstance& person,
                              const gl::game::PersonMovement& movement,
                              const gl::assets::PadAnimation& animation,
                              const gl::render::Camera& camera) {
    if (person.pad.empty()) return {};
    if (animation.frame_width <= 0 || animation.frame_height <= 0) return {};
    const PersonPlacement placement =
        place_person_px(animation, movement.direction8, movement.x, movement.y);
    if (placement.row >= animation.crops.size()) return {};
    const SDL_FRect rect =
        gl::render::to_screen(camera, placement.draw_x, placement.draw_y,
                              static_cast<float>(animation.frame_width),
                              static_cast<float>(animation.frame_height));
    return PersonHitRect{true, rect, placement.row};
}

Rect hero_marker_rect(const gl::game::Hero& hero, const gl::render::Camera& camera) {
    const float zoom = camera.zoom > 0.0f ? camera.zoom : 1.0f;
    const float size = static_cast<float>(gl::game::kHeroMarkerSize) * zoom;
    return Rect{(hero.x - camera.x) * zoom - size * 0.5f,
                (hero.y - camera.y) * zoom - size * 0.5f, size, size};
}

PersonDrawKey hero_draw_key(const gl::game::Hero& hero) {
    return PersonDrawKey{hero.x, hero.y};
}

std::vector<std::size_t> sort_body_keys(std::span<const PersonDrawKey> person_keys,
                                        const std::optional<PersonDrawKey>& hero_key) {
    std::vector<PersonDrawKey> keys(person_keys.begin(), person_keys.end());
    if (hero_key) keys.push_back(*hero_key);
    return sort_person_indices(keys);
}

std::optional<std::size_t> hit_test_persons(std::span<const PersonHitTarget> targets,
                                            int sx, int sy) {
    const PersonHitTarget* best = nullptr;
    float best_distance = 0.0f;
    const float screen_x = static_cast<float>(sx);
    const float screen_y = static_cast<float>(sy);
    for (const PersonHitTarget& target : targets) {
        if (!(screen_x >= target.rect.x && screen_x < target.rect.x + target.rect.w &&
              screen_y >= target.rect.y && screen_y < target.rect.y + target.rect.h)) {
            continue;
        }
        const float dx = screen_x - target.center_x;
        const float dy = screen_y - (target.rect.y + target.rect.h * 0.5f);
        const float distance = dx * dx + dy * dy;
        if (best == nullptr || target.baseline > best->baseline ||
            (target.baseline == best->baseline && distance < best_distance) ||
            (target.baseline == best->baseline && distance == best_distance &&
             target.person < best->person)) {
            best = &target;
            best_distance = distance;
        }
    }
    if (best == nullptr) return std::nullopt;
    return best->person;
}

ClickTarget click_target(std::span<const PersonHitTarget> persons,
                         const gl::render::Camera& camera, const gl::game::WalkGrid& grid,
                         int sx, int sy) {
    ClickTarget target;
    target.person = hit_test_persons(persons, sx, sy);
    gl::game::Tile tile{};
    if (world_to_tile(camera, grid, sx, sy, tile)) target.tile = tile;
    return target;
}

LevelScreen::LevelScreen(gl::game::Level level, gl::render::Texture background)
    : level_(std::move(level)), background_(std::move(background)) {}

LevelScreen::LevelScreen(LevelScreen&& other) noexcept
    : level_(std::move(other.level_)),
      background_(std::move(other.background_)),
      statics_(std::move(other.statics_)),
      animations_(std::move(other.animations_)),
      persons_(std::move(other.persons_)),
      masks_(std::move(other.masks_)),
      hero_base_(std::move(other.hero_base_)),
      hero_go_base_(std::move(other.hero_go_base_)),
      hero_armor_(std::move(other.hero_armor_)),
      hero_legs_(std::move(other.hero_legs_)),
      person_keys_(std::move(other.person_keys_)),
      person_order_(std::move(other.person_order_)),
      mask_order_(std::move(other.mask_order_)),
      simulation_(std::move(other.simulation_)),
      simulated_ms_(other.simulated_ms_),
      last_simulation_ms_(other.last_simulation_ms_),
      camera_(other.camera_),
      screen_w_(other.screen_w_),
      screen_h_(other.screen_h_) {
    if (other.hero_) {
        hero_.emplace(level_, &simulation_);
        hero_->controller.spawn_at_first_entrance();
    }
}

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

    screen.masks_.reserve(screen.level_.masks.size());
    for (const gl::game::LevelMask& mask : screen.level_.masks) {
        gl::assets::Image foreground =
            compose_mask_foreground(screen.level_.background, mask.image, mask.x, mask.y);
        if (foreground.rgba.empty()) continue;
        auto created = gl::render::Texture::from_image(renderer, foreground);
        if (!created) {
            gl::log(gl::LogLevel::warning,
                    "level '" + screen.level_.name + "': skipping mask " +
                        std::to_string(mask.number) + ": " + created.error().message);
            continue;
        }
        screen.masks_.push_back(MaskOverlay{
            std::move(created.value()), mask.x, mask.y,
            static_cast<float>(mask.y + static_cast<std::int32_t>(mask.image.height)),
            static_cast<float>(mask.x) + static_cast<float>(mask.image.width) / 2.0f});
    }
    screen.mask_order_.resize(screen.masks_.size());
    for (std::size_t i = 0; i < screen.mask_order_.size(); ++i) screen.mask_order_[i] = i;
    std::stable_sort(screen.mask_order_.begin(), screen.mask_order_.end(),
                     [&screen](std::size_t a, std::size_t b) {
                         const MaskOverlay& ma = screen.masks_[a];
                         const MaskOverlay& mb = screen.masks_[b];
                         if (ma.baseline != mb.baseline) return ma.baseline < mb.baseline;
                         return ma.center_x < mb.center_x;
                     });

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

    screen.ensure_hero();

    gl::assets::Image hero_base;
    if (decode_person_image(vfs, "wear/noweapon_thrw/animation/tb_stay.csx", hero_base)) {
        auto texture = gl::render::Texture::from_image(renderer, hero_base);
        if (texture) {
            screen.hero_base_.emplace(std::move(texture.value()));
        } else {
            gl::log(gl::LogLevel::warning,
                    "level '" + screen.level_.name + "': cannot upload hero base: " +
                        texture.error().message);
        }
    } else {
        gl::log(gl::LogLevel::warning,
                "level '" + screen.level_.name + "': cannot decode hero base");
    }

    gl::assets::Image hero_go_base;
    if (decode_person_image(vfs, "wear/noweapon_thrw/animation/tb_go.csx",
                            hero_go_base)) {
        auto texture = gl::render::Texture::from_image(renderer, hero_go_base);
        if (texture) {
            screen.hero_go_base_.emplace(std::move(texture.value()));
        } else {
            gl::log(gl::LogLevel::warning,
                    "level '" + screen.level_.name + "': cannot upload hero walk base: " +
                        texture.error().message);
        }
    } else {
        gl::log(gl::LogLevel::warning,
                "level '" + screen.level_.name + "': cannot decode hero walk base");
    }

    gl::assets::Image hero_armor;
    if (decode_person_image(
            vfs,
            "wear/noweapon_thrw/armor/dospeh_almaz/animation/tb_stay.csx",
            hero_armor)) {
        auto texture = gl::render::Texture::from_image(renderer, hero_armor);
        if (texture) {
            screen.hero_armor_.emplace(std::move(texture.value()));
        } else {
            gl::log(gl::LogLevel::warning,
                    "level '" + screen.level_.name + "': cannot upload hero armor: " +
                        texture.error().message);
        }
    } else {
        gl::log(gl::LogLevel::warning,
                "level '" + screen.level_.name + "': cannot decode hero armor");
    }

    gl::assets::Image hero_legs;
    if (decode_person_image(
            vfs, "wear/noweapon_thrw/elbarms/ponogi_goluboy/animation/tb_stay.csx",
            hero_legs)) {
        auto texture = gl::render::Texture::from_image(renderer, hero_legs);
        if (texture) {
            screen.hero_legs_.emplace(std::move(texture.value()));
        } else {
            gl::log(gl::LogLevel::warning,
                    "level '" + screen.level_.name + "': cannot upload hero legs: " +
                        texture.error().message);
        }
    } else {
        gl::log(gl::LogLevel::warning,
                "level '" + screen.level_.name + "': cannot decode hero legs");
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

    if (options.draw_persons && !persons_.empty()) {
        const std::uint64_t target = elapsed_ms;
        if (target > last_simulation_ms_) {
            simulation_.advance(static_cast<double>(target - last_simulation_ms_) / 1000.0);
            last_simulation_ms_ = target;
        }
    }
    simulated_ms_ = simulation_.time_ms();

    const bool draw_hero = options.draw_hero;
    if (draw_hero) ensure_hero();

    if (options.draw_persons || draw_hero) {
        person_keys_.clear();
        person_keys_.reserve(persons_.size());
        if (options.draw_persons) {
            for (const Person& person : persons_) {
                const gl::game::PersonMovement& movement =
                    simulation_.persons()[person.instance];
                person_keys_.push_back(PersonDrawKey{movement.x, movement.y});
            }
        }
        std::optional<PersonDrawKey> hero_key;
        if (draw_hero && hero_) {
            hero_key = hero_draw_key(hero_->controller.hero());
        }
        person_order_ = sort_body_keys(person_keys_, hero_key);
        const std::size_t hero_sentinel = person_keys_.size();
        const bool hero_has_key = hero_key.has_value();
        const std::size_t mask_count = options.draw_masks ? mask_order_.size() : 0;
        const auto draw_pass = [&](PersonPass pass) {
            std::size_t person_index = 0;
            std::size_t mask_index = 0;
            while (person_index < person_order_.size() || mask_index < mask_count) {
                bool draw_person_next = mask_index >= mask_count;
                if (!draw_person_next && person_index < person_order_.size()) {
                    const std::size_t candidate = person_order_[person_index];
                    const PersonDrawKey key =
                        (hero_has_key && candidate == hero_sentinel)
                            ? *hero_key
                            : person_keys_[candidate];
                    const MaskOverlay& mask = masks_[mask_order_[mask_index]];
                    draw_person_next =
                        key.y < mask.baseline ||
                        (key.y == mask.baseline && key.x <= mask.center_x);
                }
                if (draw_person_next) {
                    const std::size_t key_index = person_order_[person_index];
                    if (hero_has_key && key_index == hero_sentinel) {
                        if (pass == PersonPass::body) {
                            if (options.draw_hero_base && hero_base_) {
                                draw_hero_base(renderer, elapsed_ms,
                                               options.draw_hero_armor);
                            } else {
                                draw_hero_marker(renderer);
                            }
                        }
                    } else if (options.draw_persons) {
                        draw_person(renderer, persons_[key_index], simulated_ms_, pass);
                    }
                    ++person_index;
                } else {
                    const MaskOverlay& mask = masks_[mask_order_[mask_index]];
                    gl::render::draw_texture(renderer, mask.texture, camera_,
                                             static_cast<float>(mask.x),
                                             static_cast<float>(mask.y), screen_w_,
                                             screen_h_);
                    ++mask_index;
                }
            }
        };
        if (options.draw_persons) draw_pass(PersonPass::shadow);
        draw_pass(PersonPass::body);
    }

    if (options.walkable_overlay) {
        draw_walkable_overlay(renderer);
    }

    if (options.debug_overlay) {
        draw_debug_overlay(renderer, screen_w_, screen_h_, elapsed_ms);
    }
}

void LevelScreen::draw_person(SDL_Renderer* renderer, const Person& person,
                              std::uint64_t elapsed_ms, PersonPass pass) const {
    if (person.instance >= level_.persons.size()) return;
    if (person.instance >= simulation_.persons().size()) return;
    const gl::game::PersonInstance& instance = level_.persons[person.instance];
    const gl::game::PersonMovement& movement = simulation_.persons()[person.instance];
    const bool moving = movement.moving && person.go_texture.has_value() &&
                        person.go_animation < instance.pad.size();
    const gl::assets::PadAnimation& animation =
        moving ? instance.pad[person.go_animation] : instance.pad[person.animation];
    const std::uint8_t direction = moving ? movement.direction16 : movement.direction8;
    const std::uint64_t frame_elapsed =
        elapsed_ms > movement.state_started_ms ? elapsed_ms - movement.state_started_ms : 0;
    const std::size_t column =
        person_frame_index(frame_elapsed, animation.delay, animation.frames_per_row);

    if (pass == PersonPass::shadow) {
        const std::optional<gl::render::Texture>& shadow_texture =
            moving ? person.go_shadow : person.shadow;
        if (!shadow_texture) return;
        const PersonShadowPlacement shadow =
            place_person_shadow_px(animation, direction, movement.x, movement.y);
        if (shadow.row >= animation.shadow_crops.size()) return;
        const FrameRect shadow_rect = frame_rect(
            column, shadow.row, animation.shadow_width, animation.shadow_height);
        const SDL_Rect source{shadow_rect.x, shadow_rect.y, shadow_rect.w, shadow_rect.h};
        const SDL_FRect destination =
            gl::render::to_screen(camera_, shadow.draw_x, shadow.draw_y,
                                  static_cast<float>(animation.shadow_width),
                                  static_cast<float>(animation.shadow_height));
        if (rect_visible(destination, screen_w_, screen_h_)) {
            draw_source(renderer, *shadow_texture, source, destination);
        }
        return;
    }

    const PersonPlacement placement =
        place_person_px(animation, direction, movement.x, movement.y);
    if (placement.row >= animation.crops.size()) return;
    const gl::render::Texture& texture = moving ? *person.go_texture : person.texture;
    const FrameRect body_rect =
        frame_rect(column, placement.row, animation.frame_width, animation.frame_height);
    const SDL_Rect source{body_rect.x, body_rect.y, body_rect.w, body_rect.h};
    const SDL_FRect destination =
        gl::render::to_screen(camera_, placement.draw_x, placement.draw_y,
                              static_cast<float>(animation.frame_width),
                              static_cast<float>(animation.frame_height));
    if (!rect_visible(destination, screen_w_, screen_h_)) return;

    draw_source(renderer, texture, source, destination,
                placement.mirror ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

ClickTarget LevelScreen::click_target(int sx, int sy) const {
    return gl::app::click_target(person_hit_targets(), camera_, level_.walkable, sx, sy);
}

std::optional<std::size_t> LevelScreen::person_at_screen(int sx, int sy) const {
    return hit_test_persons(person_hit_targets(), sx, sy);
}

std::vector<PersonHitTarget> LevelScreen::person_hit_targets() const {
    std::vector<PersonHitTarget> targets;
    targets.reserve(persons_.size());
    for (const Person& person : persons_) {
        if (person.instance >= level_.persons.size()) continue;
        if (person.instance >= simulation_.persons().size()) continue;
        const gl::game::PersonInstance& instance = level_.persons[person.instance];
        if (person.animation >= instance.pad.size()) continue;
        const gl::game::PersonMovement& movement =
            simulation_.persons()[person.instance];
        const PersonHitRect hit = person_hit_rect(
            instance, movement, instance.pad[person.animation], camera_);
        if (!hit.valid) continue;
        targets.push_back(PersonHitTarget{person.instance, hit.rect, movement.y,
                                          hit.rect.x + hit.rect.w * 0.5f});
    }
    return targets;
}

bool LevelScreen::world_to_tile(int sx, int sy, gl::game::Tile& out) const {
    return gl::app::world_to_tile(camera_, level_.walkable, sx, sy, out);
}

void LevelScreen::ensure_hero() const {
    if (hero_ && hero_->level_ptr == &level_ &&
        hero_->simulation_ptr == &simulation_) {
        return;
    }
    hero_.reset();
    hero_.emplace(level_, &simulation_);
    hero_->controller.spawn_at_first_entrance();
}

gl::game::HeroController& LevelScreen::hero() {
    ensure_hero();
    return hero_->controller;
}

const gl::game::HeroController& LevelScreen::hero() const {
    ensure_hero();
    return hero_->controller;
}

void LevelScreen::draw_hero_marker(SDL_Renderer* renderer) const {
    if (!hero_ || !(camera_.zoom > 0.0f)) return;
    const Rect rect = hero_marker_rect(hero_->controller.hero(), camera_);
    if (!(rect.w > 0.0f) || !(rect.h > 0.0f)) return;
    const float radius = rect.w * 0.5f;
    const float center_x = rect.x + radius;
    const float center_y = rect.y + rect.h * 0.5f;

    Uint8 old_r = 0;
    Uint8 old_g = 0;
    Uint8 old_b = 0;
    Uint8 old_a = 0;
    SDL_GetRenderDrawColor(renderer, &old_r, &old_g, &old_b, &old_a);
    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 180);
    SDL_RenderDrawLineF(renderer, center_x, center_y - radius, center_x + radius,
                        center_y);
    SDL_RenderDrawLineF(renderer, center_x + radius, center_y, center_x,
                        center_y + radius);
    SDL_RenderDrawLineF(renderer, center_x, center_y + radius, center_x - radius,
                        center_y);
    SDL_RenderDrawLineF(renderer, center_x - radius, center_y, center_x,
                        center_y - radius);

    SDL_SetRenderDrawColor(renderer, old_r, old_g, old_b, old_a);
    SDL_SetRenderDrawBlendMode(renderer, old_blend);
}

void LevelScreen::draw_hero_base(SDL_Renderer* renderer, std::uint64_t elapsed_ms,
                                 bool draw_armor) const {
    if (!hero_ || !(camera_.zoom > 0.0f)) return;
    const gl::game::Hero& hero = hero_->controller.hero();
    const bool moving = hero.has_goal && !hero.path.empty() && hero_go_base_.has_value();
    if (!moving && !hero_base_) return;
    const gl::render::Texture& texture = moving ? *hero_go_base_ : *hero_base_;

    int frame_width = 0;
    int frame_height = 0;
    int columns = 0;
    int delay_ms = 0;
    gl::assets::PadDirectionRow direction{};
    if (moving) {
        frame_width = 60;
        frame_height = 95;
        columns = 13;
        delay_ms = 80;
        direction = gl::assets::pad_person_row(
            gl::assets::PadAnimationType::tb_go,
            gl::game::direction8_to16(hero.direction));
    } else {
        frame_width = 48;
        frame_height = 92;
        columns = 12;
        delay_ms = 140;
        direction =
            gl::assets::pad_person_row(gl::assets::PadAnimationType::tb_stay,
                                       hero.direction);
        direction.row = 4 - direction.row;
    }

    const std::size_t column =
        static_cast<std::size_t>((elapsed_ms / delay_ms) % columns);
    const SDL_Rect source{static_cast<int>(column) * frame_width,
                          static_cast<int>(direction.row) * frame_height,
                          frame_width, frame_height};
    const SDL_FRect destination =
        gl::render::to_screen(camera_, hero.x - frame_width * 0.5f,
                              hero.y - frame_height, static_cast<float>(frame_width),
                              static_cast<float>(frame_height));
    if (!rect_visible(destination, screen_w_, screen_h_)) return;
    draw_source(renderer, texture, source, destination,
                direction.mirror ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);

    if (!draw_armor || moving || !hero_armor_) return;
    constexpr int kArmorFrameWidth = 36;
    constexpr int kArmorFrameHeight = 37;
    constexpr int kArmorOffsetX = 7;
    constexpr int kArmorOffsetY[5] = {16, 18, 27, 15, 16};
    if (direction.row >= 5) return;
    const SDL_Rect armor_source{static_cast<int>(column) * kArmorFrameWidth,
                                static_cast<int>(direction.row) * kArmorFrameHeight,
                                kArmorFrameWidth, kArmorFrameHeight};
    const SDL_FRect armor_destination = gl::render::to_screen(
        camera_, hero.x - frame_width * 0.5f + kArmorOffsetX,
        hero.y - frame_height + kArmorOffsetY[direction.row],
        static_cast<float>(kArmorFrameWidth), static_cast<float>(kArmorFrameHeight));
    draw_source(renderer, *hero_armor_, armor_source, armor_destination,
                direction.mirror ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);

    if (!hero_legs_) return;
    constexpr int kLegsFrameWidth = 36;
    constexpr int kLegsFrameHeight = 51;
    constexpr int kLegsColumns = 11;
    constexpr int kLegsOffsetX = 9;
    constexpr int kLegsOffsetY[5] = {36, 37, 38, 38, 37};
    const std::size_t legs_column = column % kLegsColumns;
    const SDL_Rect legs_source{static_cast<int>(legs_column) * kLegsFrameWidth,
                               static_cast<int>(direction.row) * kLegsFrameHeight,
                               kLegsFrameWidth, kLegsFrameHeight};
    const SDL_FRect legs_destination = gl::render::to_screen(
        camera_, hero.x - frame_width * 0.5f + kLegsOffsetX,
        hero.y - frame_height + kLegsOffsetY[direction.row],
        static_cast<float>(kLegsFrameWidth), static_cast<float>(kLegsFrameHeight));
    draw_source(renderer, *hero_legs_, legs_source, legs_destination,
                direction.mirror ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
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
