#include "gl/game/hero.hpp"

#include "gl/game/person_movement.hpp"

#include <cmath>

namespace gl::game {
namespace {
constexpr float kHeroMoveX = 6.0f;
constexpr float kHeroMoveY = 5.0f;
constexpr std::int32_t kHeroGoDelayMs = 80;
constexpr float kMovementDelayFactor = 0.8f;
constexpr float kArrive = 4.0f;
} // namespace

HeroController::HeroController(const Level& level, const LevelSimulation* simulation)
    : level_(level), simulation_(simulation) {}

void HeroController::spawn_at_first_entrance() {
    if (level_.entrances.empty()) {
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
    if (chosen == nullptr)
        chosen = &level_.entrances.front();
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

bool HeroController::set_goal(Tile goal) {
    (void)goal;
    return false;
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
    (void)person;
    (void)movement;
    return false;
}
} // namespace gl::game
