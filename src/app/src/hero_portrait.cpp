#include "gl/app/hero_portrait.hpp"
#include "gl/assets/vfs.hpp"
#include <algorithm>

namespace gl::app {
void HeroPortrait::clear() {
    body_.reset();
    ready_ = false;
}

bool HeroPortrait::load(SDL_Renderer *renderer, const gl::assets::GameVfs &vfs,
                        const gl::game::HeroOutfit &outfit) {
    clear();
    if (renderer == nullptr) return false;
    auto visual = gl::game::load_hero_visual(vfs, outfit);
    if (!visual) return false;
    auto it = visual->animations().find(0x4u);
    gl::assets::PadAnimationType type = gl::assets::PadAnimationType::tb_stay;
    if (it == visual->animations().end()) {
        it = visual->animations().find(0x1u);
        type = gl::assets::PadAnimationType::rt_stay;
    }
    if (it == visual->animations().end()) return false;
    auto texture = gl::render::Texture::from_image(renderer, it->second.body);
    if (!texture) return false;
    body_ = std::move(texture.value());
    header_ = it->second.header;
    animation_type_ = type;
    ready_ = true;
    return true;
}

void HeroPortrait::render(SDL_Renderer *renderer, float x, float y, float w,
                          float h) const {
    if (!ready_ || renderer == nullptr || !body_) return;
    const int frame_w = static_cast<int>(header_.frame_width);
    const int frame_h = static_cast<int>(header_.frame_height);
    if (frame_w <= 0 || frame_h <= 0) return;
    const auto row = gl::assets::pad_person_row(animation_type_, 0);
    const SDL_Rect source{0, static_cast<int>(row.row) * frame_h, frame_w, frame_h};
    int scale = std::min({3, static_cast<int>(w) / frame_w,
                          static_cast<int>(h) / frame_h});
    scale = std::max(scale, 1);
    const float draw_w = static_cast<float>(frame_w * scale);
    const float draw_h = static_cast<float>(frame_h * scale);
    const SDL_FRect dest{x + (w - draw_w) * 0.5f, y + h - draw_h, draw_w, draw_h};
    SDL_RenderCopyF(renderer, body_->get(), &source, &dest);
}
} // namespace gl::app
