#include "gl/game/door_runtime.hpp"

#include <algorithm>
#include <string>

namespace gl::game {
std::optional<LevelDoor> resolve_level_door(const gl::assets::SefDoor &door,
                                            std::span<const gl::assets::LvlDoor> lvl_doors,
                                            std::span<const gl::assets::LvlCellGroup> cell_groups) {
    const auto lvl_door =
        std::find_if(lvl_doors.begin(), lvl_doors.end(),
                     [&](const gl::assets::LvlDoor &candidate) {
                         return candidate.name == door.name;
                     });
    if (lvl_door == lvl_doors.end())
        return std::nullopt;
    const auto group =
        std::find_if(cell_groups.begin(), cell_groups.end(),
                     [&](const gl::assets::LvlCellGroup &candidate) {
                         return candidate.name == lvl_door->cell_group;
                     });
    if (group == cell_groups.end())
        return std::nullopt;

    LevelDoor resolved;
    resolved.name = door.name;
    resolved.state = door.is_opened;
    resolved.cells.reserve(group->cells.size());
    for (const auto &cell : group->cells)
        resolved.cells.emplace_back(static_cast<std::int32_t>(cell.first),
                                    static_cast<std::int32_t>(cell.second));
    return resolved;
}

void apply_door_state(const LevelDoor &door, WalkGrid &grid) {
    const bool open = door.state != 0;
    for (const auto &cell : door.cells)
        grid.set_walkable(cell.first, cell.second, open);
}

void DoorRuntime::reset(std::span<const LevelDoor> doors) {
    doors_.assign(doors.begin(), doors.end());
}

bool DoorRuntime::set(std::string_view name, std::int32_t state, WalkGrid &grid) {
    for (LevelDoor &door : doors_) {
        if (door.name != name)
            continue;
        door.state = state;
        apply_door_state(door, grid);
        return true;
    }
    return false;
}

const LevelDoor *DoorRuntime::find(std::string_view name) const {
    for (const LevelDoor &door : doors_)
        if (door.name == name)
            return &door;
    return nullptr;
}
} // namespace gl::game
