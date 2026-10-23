#pragma once
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/pathfinder.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace gl::game {
struct Hero {
    float x = 0.0f, y = 0.0f;
    Tile tile{};
    Tile goal{};
    bool has_goal = false;
    std::vector<Tile> path;
    std::size_t path_index = 0;
    std::uint8_t direction = 0;
};

class HeroController {
public:
    HeroController(const Level& level, const LevelSimulation* simulation);
    void spawn_at_first_entrance();
    void spawn_at_tile(Tile tile);
    void set_hero_world(float x, float y);
    bool set_goal(Tile goal);
    void advance(double dt_seconds);
    bool can_interact(const PersonInstance& person, const PersonMovement& movement) const;
    const Hero& hero() const { return hero_; }
private:
    const Level& level_;
    [[maybe_unused]] const LevelSimulation* simulation_;
    Hero hero_;
};
}
