#include "gl/game/level_simulation.hpp"

#include "gl/assets/log.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

namespace gl::game {
namespace {
constexpr float kTileW = 12.0f;
constexpr float kTileH = 9.0f;
constexpr float kArrive = 4.0f;
constexpr double kFixedStep = 1.0 / 60.0;

Tile tile_of(const PersonMovement &movement) {
    return Tile{static_cast<std::int32_t>(std::lround((movement.x - 6.0f) / kTileW)),
                static_cast<std::int32_t>(std::lround((movement.y - 4.0f) / kTileH))};
}

std::uint32_t hash_name(std::string_view text) {
    std::uint32_t hash = 2166136261u;
    for (unsigned char c : text)
        hash = (hash ^ c) * 16777619u;
    return hash;
}

bool is_route_requiring(RouteType type) {
    return type == RouteType::moved || type == RouteType::moved_flip ||
           type == RouteType::random_radius || type == RouteType::random;
}
} // namespace

LevelSimulation LevelSimulation::create(const Level &level, std::uint64_t seed) {
    LevelSimulation sim;
    sim.width_ = static_cast<float>(level.width);
    sim.height_ = static_cast<float>(level.height);
    sim.states_.reserve(level.persons.size());
    bool stay_rotate_warned = false;
    for (std::size_t i = 0; i < level.persons.size(); ++i) {
        const PersonInstance &person = level.persons[i];
        State state;
        state.route_type = parse_route_type(person.route_type);
        state.route = person.route_cells;
        state.radius = person.radius > 0 ? person.radius : 13;
        state.delay_min = std::max(0, person.delay_min);
        state.delay_max = std::max(0, person.delay_max);
        if (state.delay_min > state.delay_max)
            std::swap(state.delay_min, state.delay_max);
        state.go_delay_ms = person.go_delay_ms;
        state.move_x = person.go_move_x;
        state.move_y = person.go_move_y;
        state.spawn_tile_x = person.tile_x;
        state.spawn_tile_y = person.tile_y;
        state.spawn_x = person.tile_x * kTileW + 6.0f;
        state.spawn_y = person.tile_y * kTileH + 4.0f;
        state.movement.x = state.spawn_x;
        state.movement.y = state.spawn_y;
        state.movement.target_x = state.spawn_x;
        state.movement.target_y = state.spawn_y;
        state.movement.direction8 = person.direction;
        state.movement.direction16 = direction8_to16(person.direction);
        state.rng = static_cast<std::uint32_t>(seed) ^ hash_name(level.name) ^
                    hash_name(person.name) ^ (static_cast<std::uint32_t>(i) * 0x9E3779B9u);
        if (state.rng == 0)
            state.rng = 0x12345678u;
        state.can_move = person.go_type.has_value() && state.go_delay_ms > 0 &&
                         (state.move_x > 0.0f || state.move_y > 0.0f) &&
                         (!state.route.empty() || state.route_type == RouteType::random_radius ||
                          state.route_type == RouteType::random);
        if ((state.route_type == RouteType::moved || state.route_type == RouteType::moved_flip) &&
            state.route.empty()) {
            state.can_move = false;
        }
        if (state.route_type == RouteType::stay || state.route_type == RouteType::stay_rotate) {
            state.can_move = false;
        }
        if (state.route_type == RouteType::stay_rotate && !stay_rotate_warned) {
            gl::log(gl::LogLevel::warning,
                    "level '" + level.name +
                        "': STAY_ROTATE is not implemented; persons will stay in place");
            stay_rotate_warned = true;
        }
        if ((state.route_type == RouteType::moved || state.route_type == RouteType::moved_flip) &&
            state.route.empty()) {
            gl::log(gl::LogLevel::warning, "level '" + level.name + "': person '" + person.name +
                                               "' has route type '" + person.route_type +
                                               "' but no route cells; will stand still");
        }
        if (is_route_requiring(state.route_type) && !person.go_type.has_value()) {
            gl::log(gl::LogLevel::warning, "level '" + level.name + "': person '" + person.name +
                                               "' has no _go animation; will stand still");
        }
        if (is_route_requiring(state.route_type) && person.go_type.has_value() &&
            (state.go_delay_ms <= 0 || (state.move_x <= 0.0f && state.move_y <= 0.0f))) {
            gl::log(gl::LogLevel::warning,
                    "level '" + level.name + "': person '" + person.name +
                        "' has corrupt _go data (delay " + std::to_string(state.go_delay_ms) +
                        " ms, movement " + std::to_string(state.move_x) + "x" +
                        std::to_string(state.move_y) + "); will stand still");
        }
        sim.states_.push_back(std::move(state));
    }
    sim.grid_ = level.walkable;
    for (std::size_t index = 0; index < sim.states_.size(); ++index) {
        State &state = sim.states_[index];
        if (!state.can_move)
            continue;
        PersonMovement &m = state.movement;
        const bool planned = sim.plan_target(index);
        if (!planned) {
            m.moving = false;
            m.waiting = true;
            m.wait_until_ms = static_cast<std::uint64_t>(
                std::max(0, sim.random_range(state, state.delay_min, state.delay_max)));
            continue;
        }
        if (std::fabs(m.target_x - m.x) >= 1e-3f || std::fabs(m.target_y - m.y) >= 1e-3f)
            continue;
        m.moving = false;
        const std::int32_t delay = sim.random_range(state, state.delay_min, state.delay_max);
        m.state_started_ms = 0;
        if (delay < 500 && sim.plan_target(index))
            continue;
        m.waiting = true;
        m.wait_until_ms = static_cast<std::uint64_t>(std::max(0, delay));
    }
    sim.persons_.reserve(sim.states_.size());
    for (const State &state : sim.states_)
        sim.persons_.push_back(state.movement);
    return sim;
}

void LevelSimulation::advance(double dt_seconds) {
    if (!(dt_seconds > 0.0))
        return;
    accumulator_ += dt_seconds;
    while (accumulator_ + 1e-12 >= kFixedStep) {
        accumulator_ -= kFixedStep;
        time_ms_exact_ += kFixedStep * 1000.0;
        time_ms_ = static_cast<std::uint64_t>(std::llround(time_ms_exact_));
        rebuild_occupancy();
        const auto pause = [&](State &state, std::int32_t delay) {
            state.movement.waiting = true;
            state.movement.wait_until_ms =
                time_ms_ + static_cast<std::uint64_t>(std::max(0, delay));
        };
        const auto finish_target = [&](State &state, std::size_t index) {
            const std::int32_t delay = random_range(state, state.delay_min, state.delay_max);
            state.movement.state_started_ms = time_ms_;
            if (delay < 500 && plan_target(index))
                return;
            pause(state, delay);
        };
        for (std::size_t index = 0; index < states_.size(); ++index) {
            State &state = states_[index];
            if (!state.can_move)
                continue;
            PersonMovement &m = state.movement;
            if (m.moving) {
                if (m.path_index >= m.path.size()) {
                    m.moving = false;
                } else {
                    const Tile waypoint = m.path[m.path_index];
                    m.target_x = waypoint.x * kTileW + 6.0f;
                    m.target_y = waypoint.y * kTileH + 4.0f;
                    bool occupied_now = false;
                    if (!grid_.empty() && waypoint != tile_of(m) && waypoint.x >= 0 &&
                        waypoint.y >= 0 && waypoint.x < grid_.width && waypoint.y < grid_.height)
                        occupied_now =
                            occupancy_[static_cast<std::size_t>(waypoint.y) * grid_.width +
                                       waypoint.x] != 0;
                    if (occupied_now) {
                        m.moving = false;
                    } else {
                        const float dx = m.target_x - m.x;
                        const float dy = m.target_y - m.y;
                        const float distance = std::sqrt(dx * dx + dy * dy);
                        const float speed = iso_speed(dx, dy, state.move_x, state.move_y) *
                                            1000.0f / static_cast<float>(state.go_delay_ms);
                        const float step = speed * static_cast<float>(kFixedStep);
                        if (distance <= step ||
                            (std::fabs(dx) < kArrive && std::fabs(dy) < kArrive)) {
                            m.x = m.target_x;
                            m.y = m.target_y;
                            if (m.path_index + 1 < m.path.size()) {
                                ++m.path_index;
                            } else {
                                m.moving = false;
                                finish_target(state, index);
                            }
                        } else {
                            m.x += dx / distance * step;
                            m.y += dy / distance * step;
                            m.direction16 = direction16_from_velocity(dx, dy);
                            m.direction8 = direction16_to8(m.direction16);
                        }
                        if (m.moving && !grid_.empty() && m.path_index < m.path.size()) {
                            const Tile reserved = m.path[m.path_index];
                            if (reserved.x >= 0 && reserved.y >= 0 && reserved.x < grid_.width &&
                                reserved.y < grid_.height)
                                occupancy_[static_cast<std::size_t>(reserved.y) * grid_.width +
                                           reserved.x] = 1;
                        }
                    }
                }
            } else if (m.waiting) {
                if (time_ms_ >= m.wait_until_ms) {
                    m.waiting = false;
                    if (!plan_target(index))
                        pause(state, random_range(state, state.delay_min, state.delay_max));
                }
            } else {
                if (!plan_target(index))
                    pause(state, random_range(state, state.delay_min, state.delay_max));
            }
            persons_[index] = m;
        }
    }
}

bool LevelSimulation::can_move(std::size_t index) const {
    if (index >= states_.size())
        return false;
    const State &state = states_[index];
    return state.can_move && state.route_type != RouteType::stay &&
           state.route_type != RouteType::stay_rotate;
}

bool LevelSimulation::plan_target(std::size_t index) {
    State &state = states_[index];
    PersonMovement &m = state.movement;
    if (state.route_type == RouteType::stay || state.route_type == RouteType::stay_rotate) {
        state.can_move = false;
        return false;
    }
    std::int32_t tile_x = 0;
    std::int32_t tile_y = 0;
    if (state.route_type == RouteType::moved || state.route_type == RouteType::moved_flip) {
        const std::int32_t count = static_cast<std::int32_t>(state.route.size());
        if (state.route_index < 0 || state.route_index >= count)
            state.route_index = 0;
        tile_x = state.route[static_cast<std::size_t>(state.route_index)].first;
        tile_y = state.route[static_cast<std::size_t>(state.route_index)].second;
        if (state.route_type == RouteType::moved) {
            state.route_index = (state.route_index + 1) % count;
        } else {
            state.route_index += ((next_random(state) & 1u) != 0) ? 1 : -1;
        }
    } else {
        const bool around_spawn = state.route_type == RouteType::random_radius;
        const float base_x = around_spawn ? state.spawn_x : m.x;
        const float base_y = around_spawn ? state.spawn_y : m.y;
        const std::int32_t base_tile_x =
            around_spawn ? state.spawn_tile_x
                         : static_cast<std::int32_t>(std::floor(base_x / kTileW));
        const std::int32_t base_tile_y =
            around_spawn ? state.spawn_tile_y
                         : static_cast<std::int32_t>(std::floor(base_y / kTileH));
        tile_x = base_tile_x;
        tile_y = base_tile_y;
        for (int attempt = 0; attempt < 64; ++attempt) {
            const std::int32_t candidate_x =
                base_tile_x + random_range(state, -state.radius, state.radius);
            const std::int32_t candidate_y =
                base_tile_y + random_range(state, -state.radius, state.radius);
            bool candidate_ok = candidate_x >= 0 && candidate_y >= 0 &&
                                candidate_x * kTileW + 6.0f < width_ &&
                                candidate_y * kTileH + 4.0f < height_;
            if (candidate_ok && !grid_.empty()) {
                candidate_ok = grid_.is_walkable(candidate_x, candidate_y) &&
                               (occupancy_.empty() ||
                                occupancy_[static_cast<std::size_t>(candidate_y) * grid_.width +
                                           candidate_x] == 0);
            }
            if (candidate_ok) {
                tile_x = candidate_x;
                tile_y = candidate_y;
                break;
            }
        }
    }
    const Tile goal{tile_x, tile_y};
    const Tile from = tile_of(m);
    const std::vector<std::uint8_t> blocked = blocked_for(index);
    std::vector<Tile> path;
    if (grid_.empty() || line_of_sight(grid_, from, goal, blocked)) {
        path = {from, goal};
    } else {
        path = find_path(grid_, from, goal, blocked);
    }
    if (path.size() < 2)
        return false;
    m.path = std::move(path);
    m.path_index = 1;
    m.target_x = m.path[1].x * kTileW + 6.0f;
    m.target_y = m.path[1].y * kTileH + 4.0f;
    m.moving = true;
    m.state_started_ms = time_ms_;
    if (!occupancy_.empty()) {
        const Tile reserved = m.path.back();
        if (reserved.x >= 0 && reserved.y >= 0 && reserved.x < grid_.width &&
            reserved.y < grid_.height)
            occupancy_[static_cast<std::size_t>(reserved.y) * grid_.width + reserved.x] = 1;
    }
    return true;
}

void LevelSimulation::rebuild_occupancy() {
    occupancy_.clear();
    if (grid_.empty())
        return;
    occupancy_.assign(
        static_cast<std::size_t>(grid_.width) * static_cast<std::size_t>(grid_.height), 0);
    for (const State &state : states_) {
        const Tile tile = tile_of(state.movement);
        if (tile.x < 0 || tile.y < 0 || tile.x >= grid_.width || tile.y >= grid_.height)
            continue;
        occupancy_[static_cast<std::size_t>(tile.y) * grid_.width + tile.x] = 1;
    }
}

std::vector<std::uint8_t> LevelSimulation::blocked_for(std::size_t index) const {
    std::vector<std::uint8_t> blocked = occupancy_;
    if (blocked.empty())
        return blocked;
    const Tile self = tile_of(states_[index].movement);
    if (self.x >= 0 && self.y >= 0 && self.x < grid_.width && self.y < grid_.height)
        blocked[static_cast<std::size_t>(self.y) * grid_.width + self.x] = 0;
    return blocked;
}

std::uint32_t LevelSimulation::next_random(State &state) {
    state.rng ^= state.rng << 13;
    state.rng ^= state.rng >> 17;
    state.rng ^= state.rng << 5;
    return state.rng;
}

std::int32_t LevelSimulation::random_range(State &state, std::int32_t low, std::int32_t high) {
    if (high <= low)
        return low;
    const std::uint32_t span = static_cast<std::uint32_t>(high - low) + 1u;
    return low + static_cast<std::int32_t>(next_random(state) % span);
}
} // namespace gl::game
