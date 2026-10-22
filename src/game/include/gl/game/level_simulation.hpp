#pragma once
#include "gl/game/level.hpp"
#include "gl/game/pathfinder.hpp"
#include "gl/game/person_movement.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace gl::game {
struct PersonMovement {
    float x = 0.0f;
    float y = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
    std::uint8_t direction8 = 0;
    std::uint8_t direction16 = 0;
    bool moving = false;
    bool waiting = false;
    std::uint64_t wait_until_ms = 0;
    std::uint64_t state_started_ms = 0;
    std::vector<Tile> path;
    std::size_t path_index = 0;
};

class LevelSimulation {
  public:
    static LevelSimulation create(const Level &level, std::uint64_t seed);
    void advance(double dt_seconds);
    const std::vector<PersonMovement> &persons() const { return persons_; }
    bool can_move(std::size_t index) const;
    std::uint64_t time_ms() const { return time_ms_; }

  private:
    struct State {
        PersonMovement movement;
        std::vector<std::pair<std::int32_t, std::int32_t>> route;
        RouteType route_type = RouteType::stay;
        std::int32_t radius = 13;
        std::int32_t delay_min = 0;
        std::int32_t delay_max = 0;
        float move_x = 0.0f;
        float move_y = 0.0f;
        std::int32_t go_delay_ms = 0;
        float spawn_x = 0.0f;
        float spawn_y = 0.0f;
        std::int32_t spawn_tile_x = 0;
        std::int32_t spawn_tile_y = 0;
        std::int32_t route_index = 0;
        std::uint32_t rng = 1;
        bool can_move = false;
    };
    bool plan_target(std::size_t index);
    void rebuild_occupancy();
    std::vector<std::uint8_t> blocked_for(std::size_t index) const;
    std::uint32_t next_random(State &state);
    std::int32_t random_range(State &state, std::int32_t low, std::int32_t high);
    std::vector<State> states_;
    std::vector<PersonMovement> persons_;
    WalkGrid grid_;
    std::vector<std::uint8_t> occupancy_;
    std::uint64_t time_ms_ = 0;
    double time_ms_exact_ = 0.0;
    double accumulator_ = 0.0;
    float width_ = 0.0f;
    float height_ = 0.0f;
};
} // namespace gl::game
