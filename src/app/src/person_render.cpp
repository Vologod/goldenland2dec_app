#include "gl/app/person_render.hpp"

#include <algorithm>
#include <numeric>

namespace gl::app {
namespace {

constexpr float kTileWidth = 12.0f;
constexpr float kTileHeight = 9.0f;

}

std::size_t person_frame_index(std::uint64_t elapsed_ms, std::int32_t delay_ms,
                               std::int32_t frames_per_row) {
    if (frames_per_row <= 0) return 0;
    const std::uint64_t delay =
        delay_ms > 0 ? static_cast<std::uint64_t>(delay_ms) : std::uint64_t{1};
    return static_cast<std::size_t>((elapsed_ms / delay) %
                                    static_cast<std::uint64_t>(frames_per_row));
}

std::vector<std::size_t> sort_person_indices(std::span<const PersonDrawKey> keys) {
    std::vector<std::size_t> order(keys.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [keys](std::size_t a, std::size_t b) {
        if (keys[a].y != keys[b].y) return keys[a].y < keys[b].y;
        return keys[a].x < keys[b].x;
    });
    return order;
}

FrameRect frame_rect(std::size_t column, std::size_t row, int frame_width,
                     int frame_height) {
    if (frame_width <= 0 || frame_height <= 0) return {};
    return FrameRect{
        static_cast<int>(column) * frame_width,
        static_cast<int>(row) * frame_height,
        frame_width,
        frame_height,
    };
}

PersonPlacement place_person(const gl::assets::PadAnimation& animation,
                             std::uint8_t direction, std::int32_t tile_x,
                             std::int32_t tile_y) {
    const gl::assets::PadDirectionRow row =
        gl::assets::pad_person_row(animation.type, direction);
    return PersonPlacement{
        static_cast<float>(tile_x) * kTileWidth - static_cast<float>(animation.anchor_x),
        static_cast<float>(tile_y) * kTileHeight - static_cast<float>(animation.anchor_y),
        row.row,
        row.mirror,
    };
}

PersonShadowPlacement place_person_shadow(const gl::assets::PadAnimation& animation,
                                          std::uint8_t direction, std::int32_t tile_x,
                                          std::int32_t tile_y) {
    const gl::assets::PadDirectionRow row =
        gl::assets::pad_shadow_row(animation.type, direction);
    return PersonShadowPlacement{
        static_cast<float>(tile_x) * kTileWidth - static_cast<float>(animation.shadow_anchor_x),
        static_cast<float>(tile_y) * kTileHeight - static_cast<float>(animation.shadow_anchor_y),
        row.row,
    };
}

PersonPlacement place_person_px(const gl::assets::PadAnimation& animation,
                                std::uint8_t direction, float x, float y) {
    const gl::assets::PadDirectionRow row =
        gl::assets::pad_person_row(animation.type, direction);
    return PersonPlacement{
        x - static_cast<float>(animation.anchor_x),
        y - static_cast<float>(animation.anchor_y),
        row.row,
        row.mirror,
    };
}

PersonShadowPlacement place_person_shadow_px(const gl::assets::PadAnimation& animation,
                                             std::uint8_t direction, float x, float y) {
    const gl::assets::PadDirectionRow row =
        gl::assets::pad_shadow_row(animation.type, direction);
    return PersonShadowPlacement{
        x - static_cast<float>(animation.shadow_anchor_x),
        y - static_cast<float>(animation.shadow_anchor_y),
        row.row,
    };
}

}
