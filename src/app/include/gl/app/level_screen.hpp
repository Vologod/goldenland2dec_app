#pragma once

#include <SDL.h>

#include <gl/app/person_render.hpp>
#include <gl/assets/result.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/game/level.hpp>
#include <gl/game/level_simulation.hpp>
#include <gl/render/camera.hpp>
#include <gl/render/texture.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace gl::app {

struct LevelScreenOptions {
    bool background_only = false;
    bool debug_overlay = false;
    bool walkable_overlay = false;
    bool draw_persons = true;
    bool draw_masks = true;
};

class LevelScreen {
public:
    static Result<LevelScreen> load(SDL_Renderer* renderer, gl::assets::GameVfs& vfs,
                                    std::string_view level_name, bool load_persons = true);

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

    LevelScreen(gl::game::Level level, gl::render::Texture background);

    float world_width() const;
    float world_height() const;
    void draw_person(SDL_Renderer* renderer, const Person& person,
                     std::uint64_t elapsed_ms, PersonPass pass) const;
    void draw_debug_overlay(SDL_Renderer* renderer, int screen_w, int screen_h,
                            std::uint64_t elapsed_ms) const;
    void draw_walkable_overlay(SDL_Renderer* renderer) const;

    gl::game::Level level_;
    gl::render::Texture background_;
    std::vector<Sprite> statics_;
    std::vector<Animation> animations_;
    std::vector<Person> persons_;
    std::vector<MaskOverlay> masks_;
    std::vector<PersonDrawKey> person_keys_;
    std::vector<std::size_t> person_order_;
    std::vector<std::size_t> mask_order_;
    gl::game::LevelSimulation simulation_;
    std::uint64_t simulated_ms_ = 0;
    std::uint64_t last_simulation_ms_ = 0;
    gl::render::Camera camera_;
    int screen_w_ = 0;
    int screen_h_ = 0;
};

}
