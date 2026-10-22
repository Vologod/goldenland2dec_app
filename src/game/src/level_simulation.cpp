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
        if (state.can_move) {
            sim.choose_target(state);
            PersonMovement &m = state.movement;
            if (std::fabs(m.target_x - m.x) < 1e-3f && std::fabs(m.target_y - m.y) < 1e-3f) {
                m.moving = false;
                const std::int32_t delay =
                    sim.random_range(state, state.delay_min, state.delay_max);
                m.state_started_ms = 0;
                if (delay < 500) {
                    sim.choose_target(state);
                } else {
                    m.waiting = true;
                    m.wait_until_ms = static_cast<std::uint64_t>(delay);
                }
            }
        }
        sim.states_.push_back(std::move(state));
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
        for (std::size_t index = 0; index < states_.size(); ++index) {
            State &state = states_[index];
            if (!state.can_move)
                continue;
            PersonMovement &m = state.movement;
            if (m.moving) {
                const float dx = m.target_x - m.x;
                const float dy = m.target_y - m.y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                const float speed = iso_speed(dx, dy, state.move_x, state.move_y) * 1000.0f /
                                    static_cast<float>(state.go_delay_ms);
                const float step = speed * static_cast<float>(kFixedStep);
                if (distance <= step || (std::fabs(dx) < kArrive && std::fabs(dy) < kArrive)) {
                    m.x = m.target_x;
                    m.y = m.target_y;
                    m.moving = false;
                    const std::int32_t delay =
                        random_range(state, state.delay_min, state.delay_max);
                    m.state_started_ms = time_ms_;
                    if (delay < 500) {
                        choose_target(state);
                    } else {
                        m.waiting = true;
                        m.wait_until_ms = time_ms_ + static_cast<std::uint64_t>(delay);
                    }
                } else {
                    m.x += dx / distance * step;
                    m.y += dy / distance * step;
                    m.direction16 = direction16_from_velocity(dx, dy);
                    m.direction8 = direction16_to8(m.direction16);
                }
            } else if (m.waiting) {
                if (time_ms_ >= m.wait_until_ms) {
                    m.waiting = false;
                    choose_target(state);
                }
            } else {
                choose_target(state);
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

void LevelSimulation::choose_target(State &state) {
    PersonMovement &m = state.movement;
    if (state.route_type == RouteType::stay || state.route_type == RouteType::stay_rotate) {
        state.can_move = false;
        return;
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
        for (int attempt = 0; attempt < 64; ++attempt) {
            const std::int32_t candidate_x =
                base_tile_x + random_range(state, -state.radius, state.radius);
            const std::int32_t candidate_y =
                base_tile_y + random_range(state, -state.radius, state.radius);
            if (candidate_x >= 0 && candidate_y >= 0 && candidate_x * kTileW + 6.0f < width_ &&
                candidate_y * kTileH + 4.0f < height_) {
                tile_x = candidate_x;
                tile_y = candidate_y;
                break;
            }
            tile_x = base_tile_x;
            tile_y = base_tile_y;
        }
    }
    m.target_x = tile_x * kTileW + 6.0f;
    m.target_y = tile_y * kTileH + 4.0f;
    m.moving = true;
    m.state_started_ms = time_ms_;
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
