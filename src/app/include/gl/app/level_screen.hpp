#pragma once

#include <gl/assets/result.hpp>
#include <gl/assets/vfs.hpp>
#include <gl/game/level.hpp>
#include <gl/render/camera.hpp>
#include <gl/render/texture.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace gl::app {

struct LevelScreenOptions {
    bool background_only = false;
    bool debug_overlay = false;
};

class LevelScreen {
public:
    static Result<LevelScreen> load(SDL_Renderer* renderer, gl::assets::GameVfs& vfs,
                                    std::string_view level_name);

    void render(SDL_Renderer* renderer, const LevelScreenOptions& options,
                std::uint64_t elapsed_ms, int screen_w, int screen_h);

    void pan(float world_dx, float world_dy);
    void zoom_at(float factor, float screen_x, float screen_y, int screen_w, int screen_h);
    void reset_camera();
    void set_viewport(int screen_w, int screen_h);

    const gl::render::Camera& camera() const { return camera_; }
    const gl::game::Level& level() const { return level_; }

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

    LevelScreen(gl::game::Level level, gl::render::Texture background);

    float world_width() const;
    float world_height() const;
    void draw_debug_overlay(SDL_Renderer* renderer, int screen_w, int screen_h,
                            std::uint64_t elapsed_ms) const;

    gl::game::Level level_;
    gl::render::Texture background_;
    std::vector<Sprite> statics_;
    std::vector<Animation> animations_;
    gl::render::Camera camera_;
    int screen_w_ = 0;
    int screen_h_ = 0;
};

}
