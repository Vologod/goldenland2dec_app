#include "gl/game/level_simulation.hpp"

#include "gl/assets/log.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace gl::game {
namespace {
constexpr float kTileW = 12.0f;
constexpr float kTileH = 9.0f;
constexpr float kArrive = 4.0f;
constexpr double kFixedStep = 1.0 / 60.0;

Tile tile_at(float x, float y) {
    return Tile{static_cast<std::int32_t>(std::lround((x - 6.0f) / kTileW)),
                static_cast<std::int32_t>(std::lround((y - 4.0f) / kTileH))};
}

Tile tile_of(const PersonMovement &movement) { return tile_at(movement.x, movement.y); }

constexpr std::int32_t kMinBackoffMs = 500;
constexpr std::int32_t kRandomSampleSafetyLimit = 1'000'000;

bool is_route_requiring(RouteType type) {
    return type == RouteType::moved || type == RouteType::moved_flip ||
           type == RouteType::random_radius || type == RouteType::random;
}
} // namespace

LevelSimulation LevelSimulation::create(const Level &level, std::uint64_t seed) {
    LevelSimulation sim;
    sim.rng_ = MsvcRandom(static_cast<std::uint32_t>(seed));
    sim.width_ = static_cast<float>(level.width);
    sim.height_ = static_cast<float>(level.height);
    sim.level_name_ = level.name;
    sim.states_.reserve(level.persons.size());
    bool stay_rotate_warned = false;
    for (std::size_t i = 0; i < level.persons.size(); ++i) {
        const PersonInstance &person = level.persons[i];
        State state;
        state.name = person.name;
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
    sim.rebuild_occupancy();
    for (std::size_t index = 0; index < sim.states_.size(); ++index) {
        State &state = sim.states_[index];
        if (!state.can_move)
            continue;
        PersonMovement &m = state.movement;
        const std::int32_t delay = sim.rng_.range(state.delay_min, state.delay_max);
        m.state_started_ms = 0;
        if (delay >= kMinBackoffMs) {
            m.moving = false;
            m.waiting = true;
            m.wait_until_ms = static_cast<std::uint64_t>(delay);
            continue;
        }
        if (!sim.plan_target(index)) {
            m.moving = false;
            m.waiting = true;
            m.wait_until_ms = static_cast<std::uint64_t>(
                std::max(kMinBackoffMs, sim.rng_.range(state.delay_min, state.delay_max)));
            continue;
        }
        if (std::fabs(m.target_x - m.x) >= 1e-3f || std::fabs(m.target_y - m.y) >= 1e-3f)
            continue;
        sim.consume_goal(state);
        const std::int32_t retry_delay = sim.rng_.range(state.delay_min, state.delay_max);
        m.state_started_ms = 0;
        if (retry_delay < kMinBackoffMs && sim.plan_target(index))
            continue;
        m.moving = false;
        m.waiting = true;
        m.wait_until_ms = static_cast<std::uint64_t>(
            retry_delay < kMinBackoffMs ? kMinBackoffMs : retry_delay);
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
            const std::int32_t delay = rng_.range(state.delay_min, state.delay_max);
            consume_goal(state);
            state.movement.state_started_ms = time_ms_;
            if (delay < 500 && plan_target(index))
                return;
            pause(state, delay < 500 ? kMinBackoffMs : delay);
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
                    const float dx = m.target_x - m.x;
                    const float dy = m.target_y - m.y;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    const float speed = iso_speed(dx, dy, state.move_x, state.move_y) *
                                        1000.0f / static_cast<float>(state.go_delay_ms);
                    const float step = speed * static_cast<float>(kFixedStep);
                    const bool arrives = distance <= step ||
                                         (std::fabs(dx) < kArrive && std::fabs(dy) < kArrive);
                    float next_x = m.x;
                    float next_y = m.y;
                    if (arrives) {
                        next_x = m.target_x;
                        next_y = m.target_y;
                    } else if (distance > 0.0f) {
                        next_x = m.x + dx / distance * step;
                        next_y = m.y + dy / distance * step;
                    }
                    const Tile current = tile_of(m);
                    const Tile next = tile_at(next_x, next_y);
                    if (next != current && tile_occupied(index, next)) {
                        m.moving = false;
                    } else if (arrives) {
                        m.x = m.target_x;
                        m.y = m.target_y;
                        if (m.path_index + 1 < m.path.size()) {
                            ++m.path_index;
                        } else {
                            m.moving = false;
                            finish_target(state, index);
                        }
                    } else {
                        m.x = next_x;
                        m.y = next_y;
                        m.direction16 = direction16_from_velocity(dx, dy);
                        m.direction8 = direction16_to8(m.direction16);
                    }
                    if (m.moving && m.path_index < m.path.size())
                        mark_occupied(m.path[m.path_index]);
                    mark_occupied(tile_of(m));
                }
            } else if (m.waiting) {
                if (time_ms_ >= m.wait_until_ms) {
                    m.waiting = false;
                    if (!plan_target(index))
                        pause(state, std::max(kMinBackoffMs,
                                              rng_.range(state.delay_min, state.delay_max)));
                }
            } else {
                if (!plan_target(index))
                    pause(state, std::max(kMinBackoffMs,
                                          rng_.range(state.delay_min, state.delay_max)));
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

bool LevelSimulation::pick_goal(std::size_t index) {
    State &state = states_[index];
    if (state.has_goal)
        return true;
    if (state.route_type == RouteType::stay || state.route_type == RouteType::stay_rotate) {
        state.can_move = false;
        return false;
    }
    if (state.route_type == RouteType::moved || state.route_type == RouteType::moved_flip) {
        const std::int32_t count = static_cast<std::int32_t>(state.route.size());
        if (state.route_index < 0 || state.route_index >= count)
            state.route_index = 0;
        state.goal = Tile{state.route[static_cast<std::size_t>(state.route_index)].first,
                          state.route[static_cast<std::size_t>(state.route_index)].second};
        if (state.route_type == RouteType::moved) {
            if (count > 0)
                state.route_index = (state.route_index + 1) % count;
        } else {
            state.route_index += rng_.coin(0.5) ? 1 : -1;
        }
    } else {
        PersonMovement &m = state.movement;
        const bool around_spawn = state.route_type == RouteType::random_radius;
        const Tile base = around_spawn ? Tile{state.spawn_tile_x, state.spawn_tile_y} : tile_of(m);
        const auto in_bounds = [&](Tile tile) {
            return tile.x >= 0 && tile.y >= 0 && tile.x * kTileW + 6.0f < width_ &&
                   tile.y * kTileH + 4.0f < height_;
        };
        const auto acceptable = [&](Tile tile) {
            return in_bounds(tile) && (grid_.empty() || grid_.is_walkable(tile.x, tile.y));
        };
        if (!in_bounds(base)) {
            state.goal = base;
            state.has_goal = true;
            return true;
        }
        Tile candidate = base;
        bool found = false;
        for (std::int32_t attempt = 0; attempt < kRandomSampleSafetyLimit; ++attempt) {
            const Tile sample{base.x + rng_.range(-state.radius, state.radius),
                              base.y + rng_.range(-state.radius, state.radius)};
            if (!acceptable(sample))
                continue;
            candidate = sample;
            found = true;
            break;
        }
        if (!found) {
            if (!state.warned_degenerate_goal) {
                gl::log(gl::LogLevel::warning,
                        "level '" + level_name_ + "': person '" + state.name +
                            "': RANDOM goal sampling exhausted " +
                            std::to_string(kRandomSampleSafetyLimit) +
                            " attempts without a walkable candidate; using base tile (" +
                            std::to_string(base.x) + "," + std::to_string(base.y) + ")");
                state.warned_degenerate_goal = true;
            }
            candidate = base;
        } else {
            state.warned_degenerate_goal = false;
        }
        state.goal = candidate;
    }
    state.has_goal = true;
    return true;
}

void LevelSimulation::consume_goal(State &state) {
    if (!state.has_goal)
        return;
    state.has_goal = false;
}

bool LevelSimulation::plan_target(std::size_t index) {
    State &state = states_[index];
    PersonMovement &m = state.movement;
    const bool newly_selected = !state.has_goal;
    if (!pick_goal(index))
        return false;
    const Tile goal = state.goal;
    const Tile from = tile_of(m);
    const std::vector<std::uint8_t> blocked = blocked_for(index);
    std::vector<Tile> path;
    if (grid_.empty() || from == goal || line_of_sight(grid_, from, goal, blocked)) {
        path = {from, goal};
    } else {
        path = find_path(grid_, from, goal, blocked);
    }
    if (path.size() < 2) {
        const bool random_route =
            state.route_type == RouteType::random || state.route_type == RouteType::random_radius;
        if (random_route) {
            state.has_goal = false;
        } else if (newly_selected && (state.route_type == RouteType::moved ||
                                      state.route_type == RouteType::moved_flip)) {
            consume_goal(state);
        }
        return false;
    }
    m.path = std::move(path);
    m.path_index = 1;
    m.target_x = m.path[1].x * kTileW + 6.0f;
    m.target_y = m.path[1].y * kTileH + 4.0f;
    m.moving = true;
    m.state_started_ms = time_ms_;
    mark_occupied(m.path.back());
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

void LevelSimulation::mark_occupied(Tile tile) {
    if (occupancy_.empty() || grid_.empty())
        return;
    if (tile.x < 0 || tile.y < 0 || tile.x >= grid_.width || tile.y >= grid_.height)
        return;
    occupancy_[static_cast<std::size_t>(tile.y) * grid_.width + tile.x] = 1;
}

std::vector<std::uint8_t> LevelSimulation::blocked_for(std::size_t index) const {
    std::vector<std::uint8_t> blocked = occupancy_;
    if (blocked.empty())
        return blocked;
    const State &state = states_[index];
    const auto clear = [&](Tile tile) {
        if (tile.x < 0 || tile.y < 0 || tile.x >= grid_.width || tile.y >= grid_.height)
            return;
        blocked[static_cast<std::size_t>(tile.y) * grid_.width + tile.x] = 0;
    };
    clear(tile_of(state.movement));
    if (state.has_goal)
        clear(state.goal);
    return blocked;
}

bool LevelSimulation::tile_occupied(std::size_t index, Tile tile) const {
    if (occupancy_.empty())
        return false;
    const State &state = states_[index];
    if (tile == tile_of(state.movement))
        return false;
    if (state.has_goal && tile == state.goal)
        return false;
    return occupancy_[static_cast<std::size_t>(tile.y) * grid_.width + tile.x] != 0;
}

} // namespace gl::game

