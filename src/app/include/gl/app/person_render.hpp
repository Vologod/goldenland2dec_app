#pragma once

#include <gl/assets/pad.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gl::app {

struct PersonDrawKey {
    float x = 0.0f;
    float y = 0.0f;
};

struct FrameRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

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

std::vector<std::size_t> sort_person_indices(std::span<const PersonDrawKey> keys);

FrameRect frame_rect(std::size_t column, std::size_t row, int frame_width,
                     int frame_height);

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
