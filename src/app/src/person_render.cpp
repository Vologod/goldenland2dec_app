#include "gl/app/person_render.hpp"

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

}
