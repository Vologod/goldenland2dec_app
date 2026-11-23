#pragma once
#include <gl/assets/pad.hpp>
#include <gl/game/hero_wear.hpp>
#include <gl/render/texture.hpp>
#include <SDL.h>
#include <optional>

namespace gl::assets { class GameVfs; }

namespace gl::app {
class HeroPortrait {
  public:
    bool load(SDL_Renderer *renderer, const gl::assets::GameVfs &vfs,
              const gl::game::HeroOutfit &outfit);
    void clear();
    bool ready() const { return ready_; }
    gl::assets::PadAnimationType animation_type() const { return animation_type_; }
    void render(SDL_Renderer *renderer, float x, float y, float w, float h) const;

  private:
    std::optional<gl::render::Texture> body_;
    gl::assets::WearHadAnimation header_;
    gl::assets::PadAnimationType animation_type_ = gl::assets::PadAnimationType::tb_stay;
    bool ready_ = false;
};
} // namespace gl::app
