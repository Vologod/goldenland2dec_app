#pragma once

#include <SDL.h>

#include <gl/app/person_render.hpp>
#include <gl/assets/result.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/game/hero.hpp>
#include <gl/game/level.hpp>
#include <gl/game/level_simulation.hpp>
#include <gl/game/pathfinder.hpp>
#include <gl/render/camera.hpp>
#include <gl/render/texture.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace gl::app {

struct LevelScreenOptions {
    bool background_only = false;
    bool debug_overlay = false;
    bool walkable_overlay = false;
    bool draw_persons = true;
    bool draw_masks = true;
    bool draw_hero = true;
    bool draw_hero_base = false;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct ClickTarget {
    // Index into Level::persons / LevelSimulation::persons(), not LevelScreen::persons_.
    std::optional<std::size_t> person;
    std::optional<gl::game::Tile> tile;
};

struct PersonHitTarget {
    // Index into Level::persons / LevelSimulation::persons(), not LevelScreen::persons_.
    std::size_t person = 0;
    SDL_FRect rect{};
    float baseline = 0.0f;
    float center_x = 0.0f;
};

struct PersonHitRect {
    bool valid = false;
    SDL_FRect rect{};
    std::size_t row = 0;
};

gl::game::Tile screen_to_tile(const gl::render::Camera& camera, int sx, int sy);

bool world_to_tile(const gl::render::Camera& camera, const gl::game::WalkGrid& grid,
                   int sx, int sy, gl::game::Tile& out);

PersonHitRect person_hit_rect(const gl::game::PersonInstance& person,
                              const gl::game::PersonMovement& movement,
                              const gl::assets::PadAnimation& animation,
                              const gl::render::Camera& camera);

Rect hero_marker_rect(const gl::game::Hero& hero, const gl::render::Camera& camera);

PersonDrawKey hero_draw_key(const gl::game::Hero& hero);

std::vector<std::size_t> sort_body_keys(std::span<const PersonDrawKey> person_keys,
                                        const std::optional<PersonDrawKey>& hero_key);

std::optional<std::size_t> hit_test_persons(std::span<const PersonHitTarget> targets,
                                            int sx, int sy);

ClickTarget click_target(std::span<const PersonHitTarget> persons,
                         const gl::render::Camera& camera, const gl::game::WalkGrid& grid,
                         int sx, int sy);

class LevelScreen {
public:
    static Result<LevelScreen> load(SDL_Renderer* renderer, gl::assets::GameVfs& vfs,
                                    std::string_view level_name, bool load_persons = true);

    LevelScreen(LevelScreen&& other) noexcept;

    void render(SDL_Renderer* renderer, const LevelScreenOptions& options,
                std::uint64_t elapsed_ms);

    void pan(float world_dx, float world_dy);
    void pan_input(int x_axis, int y_axis, float delta_seconds);
    void zoom_at(float factor, float screen_x, float screen_y);
    void reset_camera();
    void set_viewport(int screen_w, int screen_h);

    const gl::render::Camera& camera() const { return camera_; }
    const gl::game::Level& level() const { return level_; }
    gl::game::LevelSimulation& simulation() { return simulation_; }

    ClickTarget click_target(int sx, int sy) const;
    std::optional<std::size_t> person_at_screen(int sx, int sy) const;
    bool world_to_tile(int sx, int sy, gl::game::Tile& out) const;
    gl::game::HeroController& hero();
    const gl::game::HeroController& hero() const;

private:
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

    struct Person {
        gl::render::Texture texture;
        std::optional<gl::render::Texture> shadow;
        std::optional<gl::render::Texture> go_texture;
        std::optional<gl::render::Texture> go_shadow;
        std::size_t instance = 0;
        std::size_t animation = 0;
        std::size_t go_animation = 0;
    };

    struct MaskOverlay {
        gl::render::Texture texture;
        std::int32_t x = 0;
        std::int32_t y = 0;
        float baseline = 0.0f;
        float center_x = 0.0f;
    };

    enum class PersonPass { shadow, body };

    struct HeroBinding {
        HeroBinding(const gl::game::Level& level,
                    const gl::game::LevelSimulation* simulation)
            : level_ptr(&level), simulation_ptr(simulation),
              controller(level, simulation) {}

        const gl::game::Level* level_ptr = nullptr;
        const gl::game::LevelSimulation* simulation_ptr = nullptr;
        gl::game::HeroController controller;
    };

    LevelScreen(gl::game::Level level, gl::render::Texture background);

    void ensure_hero() const;
    std::vector<PersonHitTarget> person_hit_targets() const;
    float world_width() const;
    float world_height() const;
    void draw_person(SDL_Renderer* renderer, const Person& person,
                     std::uint64_t elapsed_ms, PersonPass pass) const;
    void draw_hero_marker(SDL_Renderer* renderer) const;
    void draw_hero_base(SDL_Renderer* renderer, std::uint64_t elapsed_ms) const;
    void draw_debug_overlay(SDL_Renderer* renderer, int screen_w, int screen_h,
                            std::uint64_t elapsed_ms) const;
    void draw_walkable_overlay(SDL_Renderer* renderer) const;

    gl::game::Level level_;
    gl::render::Texture background_;
    std::vector<Sprite> statics_;
    std::vector<Animation> animations_;
    std::vector<Person> persons_;
    std::vector<MaskOverlay> masks_;
    std::optional<gl::render::Texture> hero_base_;
    std::optional<gl::render::Texture> hero_go_base_;
    std::vector<PersonDrawKey> person_keys_;
    std::vector<std::size_t> person_order_;
    std::vector<std::size_t> mask_order_;
    gl::game::LevelSimulation simulation_;
    mutable std::optional<HeroBinding> hero_;
    std::uint64_t simulated_ms_ = 0;
    std::uint64_t last_simulation_ms_ = 0;
    gl::render::Camera camera_;
    int screen_w_ = 0;
    int screen_h_ = 0;
};

}
