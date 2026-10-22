#include "gl/game/pathfinder.hpp"
#include <algorithm>
#include <cmath>
#include <queue>

namespace gl::game {
namespace {

bool blocked_at(const WalkGrid& grid, const std::vector<std::uint8_t>& blocked, Tile tile) {
    if (grid.empty()) return false;
    if (!grid.is_walkable(tile.x, tile.y)) return true;
    const std::size_t index = static_cast<std::size_t>(tile.y) * grid.width + tile.x;
    return index < blocked.size() && blocked[index] != 0;
}

bool step_allowed(const WalkGrid& grid, const std::vector<std::uint8_t>& blocked,
                  Tile from, std::int32_t dx, std::int32_t dy) {
    return !blocked_at(grid, blocked, Tile{from.x + dx, from.y + dy});
}

float heuristic(Tile a, Tile b) {
    const std::int32_t dx = std::abs(a.x - b.x);
    const std::int32_t dy = std::abs(a.y - b.y);
    const std::int32_t min = std::min(dx, dy);
    const std::int32_t max = std::max(dx, dy);
    return (static_cast<float>(max) + 0.375f * static_cast<float>(min)) * 1.2f;
}

} // namespace

bool line_of_sight(const WalkGrid& grid, Tile from, Tile to,
                   const std::vector<std::uint8_t>& blocked) {
    if (from == to) return true;
    std::int32_t x = from.x;
    std::int32_t y = from.y;
    const std::int32_t dx = std::abs(to.x - from.x);
    const std::int32_t dy = std::abs(to.y - from.y);
    const std::int32_t sx = from.x < to.x ? 1 : -1;
    const std::int32_t sy = from.y < to.y ? 1 : -1;
    std::int32_t error = dx - dy;
    while (x != to.x || y != to.y) {
        const std::int32_t e2 = error * 2;
        if (e2 > -dy) { error -= dy; x += sx; }
        if (e2 < dx) { error += dx; y += sy; }
        if (blocked_at(grid, blocked, Tile{x, y})) return false;
    }
    return true;
}

std::vector<Tile> find_path(const WalkGrid& grid, Tile from, Tile to,
                            const std::vector<std::uint8_t>& blocked) {
    if (grid.empty()) return {};
    if (from.x < 0 || from.y < 0 || from.x >= grid.width || from.y >= grid.height) return {};
    if (from == to) return {from};
    if (blocked_at(grid, blocked, to)) return {};
    const std::size_t width = static_cast<std::size_t>(grid.width);
    const std::size_t height = static_cast<std::size_t>(grid.height);
    const auto index = [width](Tile t) { return static_cast<std::size_t>(t.y) * width + t.x; };
    std::vector<float> cost(width * height, -1.0f);
    std::vector<Tile> parent(width * height);
    struct Node {
        float f;
        std::uint64_t order;
        Tile tile;
    };
    const auto worse = [](const Node& a, const Node& b) {
        if (a.f != b.f) return a.f > b.f;
        return a.order > b.order;
    };
    std::priority_queue<Node, std::vector<Node>, decltype(worse)> open(worse);
    std::uint64_t order = 0;
    cost[index(from)] = 0.0f;
    parent[index(from)] = from;
    open.push({heuristic(from, to), order++, from});
    constexpr std::int32_t kSteps[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    while (!open.empty()) {
        const Node node = open.top();
        open.pop();
        if (node.tile == to) break;
        if (node.f > cost[index(node.tile)] + heuristic(node.tile, to) + 0.0001f) continue;
        for (const auto& step : kSteps) {
            if (!step_allowed(grid, blocked, node.tile, step[0], step[1])) continue;
            const Tile next{node.tile.x + step[0], node.tile.y + step[1]};
            constexpr float kStepCost = 1.0f;
            const float next_cost = cost[index(node.tile)] + kStepCost;
            if (cost[index(next)] < 0.0f || next_cost < cost[index(next)]) {
                cost[index(next)] = next_cost;
                parent[index(next)] = node.tile;
                open.push({next_cost + heuristic(next, to), order++, next});
            }
        }
    }
    if (cost[index(to)] < 0.0f) return {};
    std::vector<Tile> path;
    Tile current = to;
    while (!(current == from)) {
        path.push_back(current);
        current = parent[index(current)];
    }
    path.push_back(from);
    std::reverse(path.begin(), path.end());
    return path;
}
}
