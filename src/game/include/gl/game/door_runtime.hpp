#pragma once
#include "gl/game/level.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace gl::game {
std::optional<LevelDoor> resolve_level_door(const gl::assets::SefDoor &door,
                                            std::span<const gl::assets::LvlDoor> lvl_doors,
                                            std::span<const gl::assets::LvlCellGroup> cell_groups);
void apply_door_state(const LevelDoor &door, WalkGrid &grid);

class DoorRuntime {
  public:
    void reset(std::span<const LevelDoor> doors);
    bool set(std::string_view name, std::int32_t state, WalkGrid &grid);
    const LevelDoor *find(std::string_view name) const;
    std::size_t size() const { return doors_.size(); }
    void clear() { doors_.clear(); }

  private:
    std::vector<LevelDoor> doors_;
};
} // namespace gl::game
