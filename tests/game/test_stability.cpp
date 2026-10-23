#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
}

TEST_CASE("real data: l1_1 simulation has no jittering NPCs" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());

    auto sim = gl::game::LevelSimulation::create(level.value(), 1);
    const std::size_t count = sim.persons().size();
    std::vector<float> prev_x(count), prev_y(count), prev_dx(count), prev_dy(count);
    std::vector<int> reversals(count), toggles(count), window_reversals(count), max_window(count);
    std::vector<bool> prev_moving(count);
    for (std::size_t i = 0; i < count; ++i) {
        prev_x[i] = sim.persons()[i].x;
        prev_y[i] = sim.persons()[i].y;
        prev_moving[i] = sim.persons()[i].moving;
    }

    for (int tick = 0; tick < 60 * 180; ++tick) {
        sim.advance(1.0 / 60.0);
        for (std::size_t i = 0; i < count; ++i) {
            const auto &p = sim.persons()[i];
            const float dx = p.x - prev_x[i];
            const float dy = p.y - prev_y[i];
            if (p.moving != prev_moving[i]) {
                ++toggles[i];
                prev_moving[i] = p.moving;
            }
            const float len = std::sqrt(dx * dx + dy * dy);
            const float prev_len = std::sqrt(prev_dx[i] * prev_dx[i] + prev_dy[i] * prev_dy[i]);
            if (len > 0.05f && prev_len > 0.05f && dx * prev_dx[i] + dy * prev_dy[i] < 0.0f) {
                ++reversals[i];
                ++window_reversals[i];
            }
            if (len > 0.05f) {
                prev_dx[i] = dx;
                prev_dy[i] = dy;
            }
            prev_x[i] = p.x;
            prev_y[i] = p.y;
        }
        if ((tick + 1) % 300 == 0) {
            for (std::size_t i = 0; i < count; ++i) {
                max_window[i] = std::max(max_window[i], window_reversals[i]);
                window_reversals[i] = 0;
            }
        }
    }

    for (std::size_t i = 0; i < count; ++i) {
        const auto &p = sim.persons()[i];
        if (level->persons[i].name == "L0.P7_Kolbasens_Rodstvennik1") {
            MESSAGE("P7 final path=" << p.path.size() << " idx=" << p.path_index
                                      << " wait_until=" << p.wait_until_ms
                                      << " now=" << sim.time_ms());
        }
        MESSAGE(level->persons[i].name << " toggles=" << toggles[i] << " reversals=" << reversals[i]
                                        << " max_5s_window=" << max_window[i] << " pos=(" << p.x
                                        << "," << p.y << ") target=(" << p.target_x << ","
                                        << p.target_y << ") moving=" << p.moving
                                        << " waiting=" << p.waiting);
        CHECK(toggles[i] < 1000);
        CHECK(max_window[i] < 100);
    }
}


