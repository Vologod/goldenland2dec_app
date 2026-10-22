#pragma once

#include <gl/assets/pad.hpp>

#include <cstddef>
#include <cstdint>

namespace gl::app {

struct PersonPlacement {
    float draw_x = 0.0f;
    float draw_y = 0.0f;
    std::uint8_t row = 0;
    bool mirror = false;
};

struct PersonShadowPlacement {
    float draw_x = 0.0f;
    float draw_y = 0.0f;
    std::uint8_t row = 0;
};

std::size_t person_frame_index(std::uint64_t elapsed_ms, std::int32_t delay_ms,
                               std::int32_t frames_per_row);

PersonPlacement place_person(const gl::assets::PadAnimation& animation,
                             std::uint8_t direction, std::int32_t tile_x,
                             std::int32_t tile_y);

PersonShadowPlacement place_person_shadow(const gl::assets::PadAnimation& animation,
                                          std::uint8_t direction, std::int32_t tile_x,
                                          std::int32_t tile_y);

PersonPlacement place_person_px(const gl::assets::PadAnimation& animation,
                                std::uint8_t direction, float x, float y);

PersonShadowPlacement place_person_shadow_px(const gl::assets::PadAnimation& animation,
                                             std::uint8_t direction, float x, float y);

}
