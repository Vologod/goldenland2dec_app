#include "gl/game/hero.hpp"

#include "gl/assets/log.hpp"
#include "gl/game/person_movement.hpp"

#include <cmath>
#include <utility>

namespace gl::game {
namespace {
constexpr float kHeroMoveX = 6.0f;
constexpr float kHeroMoveY = 5.0f;
constexpr std::int32_t kHeroGoDelayMs = 80;
constexpr float kMovementDelayFactor = 0.8f;
constexpr float kArrive = 4.0f;
constexpr std::int32_t kFallbackRadius = 3;
constexpr std::int32_t kStepDirs[8][2] = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

struct WorldRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

bool rects_intersect(const WorldRect& a, const WorldRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
} // namespace

HeroController::HeroController(const Level& level, const LevelSimulation* simulation)
    : level_(level), simulation_(simulation) {}

void HeroController::spawn_at_first_entrance() {
    if (level_.entrances.empty()) {
        gl::log(gl::LogLevel::warning,
                "level '" + level_.name +
                    "': no entrances; spawning at walkable center");
        Tile center{};
        if (!level_.walkable.empty()) {
            center.x = level_.walkable.width / 2;
            center.y = level_.walkable.height / 2;
        }
        spawn_at_tile(center);
        return;
    }
    const gl::assets::SefEntrance* chosen = nullptr;
    for (const auto& entrance : level_.entrances) {
        if (level_.walkable.is_walkable(entrance.x, entrance.y)) {
            chosen = &entrance;
            break;
        }
    }
    if (chosen == nullptr) {
        gl::log(gl::LogLevel::warning,
                "level '" + level_.name +
                    "': no walkable entrance; spawning at first entrance");
        chosen = &level_.entrances.front();
    }
    spawn_at_tile({chosen->x, chosen->y});
    hero_.direction = chosen->direction;
}

void HeroController::spawn_at_tile(Tile tile) {
    hero_.tile = tile;
    hero_.x = tile_center_x(tile.x);
    hero_.y = tile_center_y(tile.y);
    hero_.goal = tile;
    hero_.has_goal = false;
    hero_.path.clear();
    hero_.path_index = 0;
}

void HeroController::set_hero_world(float x, float y) {
    hero_.x = x;
    hero_.y = y;
    hero_.tile = tile_at(x, y);
}

bool HeroController::tile_occupied_by_person(Tile tile) const {
    if (simulation_ == nullptr)
        return false;
    for (const auto& movement : simulation_->persons()) {
        if (tile_at(movement.x, movement.y) == tile)
            return true;
    }
    return false;
}

std::vector<std::uint8_t> HeroController::blocked_for(Tile target) const {
    std::vector<std::uint8_t> blocked;
    if (simulation_ == nullptr)
        return blocked;
    const std::size_t width = static_cast<std::size_t>(level_.walkable.width);
    const std::size_t height = static_cast<std::size_t>(level_.walkable.height);
    blocked.assign(width * height, 0);
    for (const auto& movement : simulation_->persons()) {
        const Tile tile = tile_at(movement.x, movement.y);
        if (tile == target)
            continue;
        if (tile.x < 0 || tile.y < 0 || tile.x >= level_.walkable.width ||
            tile.y >= level_.walkable.height)
            continue;
        blocked[static_cast<std::size_t>(tile.y) * width +
                static_cast<std::size_t>(tile.x)] = 1;
    }
    return blocked;
}

bool HeroController::set_goal(Tile goal) {
    const auto reject = [this]() {
        hero_.has_goal = false;
        hero_.path.clear();
        hero_.path_index = 0;
        hero_.goal = hero_.tile;
        return false;
    };
    hero_.has_goal = false;
    hero_.path.clear();
    hero_.path_index = 0;
    if (level_.walkable.empty())
        return reject();
    const auto in_bounds = [this](Tile tile) {
        return tile.x >= 0 && tile.y >= 0 && tile.x < level_.walkable.width &&
               tile.y < level_.walkable.height;
    };
    const auto free_cell = [this](Tile tile) {
        return level_.walkable.is_walkable(tile.x, tile.y) &&
               !tile_occupied_by_person(tile);
    };
    const auto commit = [this](Tile target, std::vector<Tile> path) {
        hero_.goal = target;
        hero_.has_goal = true;
        hero_.path = std::move(path);
        hero_.path_index = hero_.path.size() > 1 ? 1 : 0;
        return true;
    };
    if (!in_bounds(goal) || !level_.walkable.is_walkable(goal.x, goal.y))
        return reject();
    if (!tile_occupied_by_person(goal)) {
        std::vector<Tile> path = find_path(level_.walkable, hero_.tile, goal, blocked_for(goal));
        if (!path.empty())
            return commit(goal, std::move(path));
    }
    for (std::int32_t radius = 1; radius <= kFallbackRadius; ++radius) {
        for (const auto& step : kStepDirs) {
            const Tile candidate{goal.x + step[0] * radius, goal.y + step[1] * radius};
            if (!in_bounds(candidate) || !free_cell(candidate))
                continue;
            std::vector<Tile> path =
                find_path(level_.walkable, hero_.tile, candidate, blocked_for(candidate));
            if (!path.empty())
                return commit(candidate, std::move(path));
        }
    }
    return reject();
}

void HeroController::advance(double dt_seconds) {
    if (!(dt_seconds > 0.0))
        return;
    if (hero_.path_index >= hero_.path.size()) {
        hero_.path.clear();
        hero_.path_index = 0;
        hero_.has_goal = false;
        return;
    }
    double remaining = dt_seconds;
    while (remaining > 0.0 && hero_.path_index < hero_.path.size()) {
        const Tile waypoint = hero_.path[hero_.path_index];
        const float target_x = tile_center_x(waypoint.x);
        const float target_y = tile_center_y(waypoint.y);
        const float dx = target_x - hero_.x;
        const float dy = target_y - hero_.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        const float speed = iso_speed(dx, dy, kHeroMoveX, kHeroMoveY) * 1000.0f /
                            (static_cast<float>(kHeroGoDelayMs) * kMovementDelayFactor);
        const float step = speed * static_cast<float>(remaining);
        const bool arrives =
            distance <= step || (std::fabs(dx) < kArrive && std::fabs(dy) < kArrive);
        if (arrives) {
            hero_.x = target_x;
            hero_.y = target_y;
            if (distance > 0.0f)
                hero_.direction = direction16_to8(direction16_from_velocity(dx, dy));
            ++hero_.path_index;
            if (speed > 0.0f)
                remaining -= static_cast<double>(distance / speed);
            else
                remaining = 0.0;
        } else {
            hero_.x += dx / distance * step;
            hero_.y += dy / distance * step;
            hero_.direction = direction16_to8(direction16_from_velocity(dx, dy));
            remaining = 0.0;
        }
    }
    hero_.tile = tile_at(hero_.x, hero_.y);
    if (hero_.path_index >= hero_.path.size()) {
        hero_.path.clear();
        hero_.path_index = 0;
        hero_.has_goal = false;
    }
}

bool HeroController::can_interact(const PersonInstance& person,
                                  const PersonMovement& movement) const {
    const gl::assets::PadAnimation* animation = nullptr;
    for (const auto& candidate : person.pad) {
        if (candidate.type == person.stay_type) {
            animation = &candidate;
            break;
        }
    }
    if (animation == nullptr)
        return false;
    const WorldRect npc{
        movement.x - static_cast<float>(animation->anchor_x),
        movement.y - static_cast<float>(animation->anchor_y),
        static_cast<float>(animation->frame_width),
        static_cast<float>(animation->frame_height),
    };
    constexpr float kHalfMarker = static_cast<float>(kHeroMarkerSize) / 2.0f;
    const WorldRect marker{
        hero_.x - kHalfMarker,
        hero_.y - kHalfMarker,
        static_cast<float>(kHeroMarkerSize),
        static_cast<float>(kHeroMarkerSize),
    };
    return rects_intersect(marker, npc);
}
} // namespace gl::game
