#pragma once
#include "gl/assets/image.hpp"
#include "gl/assets/pad.hpp"
#include "gl/assets/person_script.hpp"
#include "gl/assets/result.hpp"
#include "gl/assets/vfs.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gl::game {
struct PersonInstance {
    std::string name;
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
    std::uint8_t direction = 0;
    std::string route_type;
    std::string route;
    std::vector<std::pair<std::int32_t, std::int32_t>> route_cells;
    std::optional<gl::assets::PadAnimationType> go_type;
    float go_move_x = 0.0f;
    float go_move_y = 0.0f;
    std::int32_t go_delay_ms = 0;
    std::int32_t radius = 0;
    std::int32_t delay_min = 0;
    std::int32_t delay_max = 0;
    std::string tribe;
    std::string dialog;
    std::string inventory;
    std::string res_name;
    gl::assets::PersonDef def;
    std::vector<gl::assets::PadAnimation> pad;
    gl::assets::PadAnimationType stay_type = gl::assets::PadAnimationType::rt_stay;
    gl::assets::Image sprite;
    gl::assets::Image shadow;
};

Result<std::vector<PersonInstance>> load_persons(gl::assets::GameVfs &vfs,
                                                 std::string_view level_name);
} // namespace gl::game
