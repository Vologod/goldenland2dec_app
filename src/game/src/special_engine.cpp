#include "gl/game/special_engine.hpp"

#include <algorithm>
#include <cstddef>

namespace gl::game {
namespace {
constexpr std::uint32_t kIdentityCount = 58;
constexpr std::uint32_t kRelativeTag = 0x80;
constexpr std::size_t kTimeSlotsRead = 3;

std::vector<gl::assets::SpecialId> build_pool(const gl::assets::SpecialDatabase &db,
                                              gl::assets::ItemType item_type) {
    std::vector<gl::assets::SpecialId> pool;
    const auto found = db.item_classes.find(item_type);
    if (found == db.item_classes.end())
        return pool;
    for (std::uint32_t id = 0; id < kIdentityCount; ++id) {
        if (std::find(found->second.begin(), found->second.end(), id) != found->second.end())
            pool.push_back(id);
    }
    return pool;
}
} // namespace

int random_special_count(std::uint32_t flags, MsvcRandom &rng) {
    if ((flags & 0x40u) == 0) {
        int n = 0;
        float p = 0.45f;
        while (rng.coin(static_cast<double>(p)) && n < 3) {
            ++n;
            p = 0.45f - 0.15f * static_cast<float>(n);
        }
        return n;
    }

    int n = 0;
    float p = 0.09f;
    while (rng.coin(static_cast<double>(p)) && n < 9) {
        ++n;
        p = static_cast<float>(9 - n) * 0.01f;
    }
    return n;
}

std::vector<gl::assets::SpecialMod> SpecialEngine::apply(const gl::assets::SpecialDatabase &db,
                                                         gl::assets::ItemType item_type, int nspec,
                                                         int loot_level, MsvcRandom &rng) {
    std::vector<gl::assets::SpecialMod> mods;
    if (nspec <= 0)
        return mods;

    std::vector<gl::assets::SpecialId> pool = build_pool(db, item_type);
    if (nspec < static_cast<int>(pool.size())) {
        const std::size_t removals = pool.size() - static_cast<std::size_t>(nspec);
        for (std::size_t removal = 0; removal < removals; ++removal) {
            const int index = rng.range(0, static_cast<int>(pool.size()) - 1);
            pool.erase(pool.begin() + index);
        }
    }

    mods.reserve(pool.size());
    for (const gl::assets::SpecialId id : pool) {
        const gl::assets::SpecialDef *def = db.find(id);
        if (def == nullptr)
            continue;

        float fval = 0.0f;
        gl::assets::SpecialFlags tag1 = 0;
        gl::assets::SpecialFlags tag2 = 0;

        int present = 0;
        for (const gl::assets::SpecialSlot &slot : def->value_slots) {
            if (slot.access != 0)
                ++present;
        }
        int pick = present > 1 ? rng.range(0, present - 1) : 0;
        for (const gl::assets::SpecialSlot &slot : def->value_slots) {
            if (slot.access == 0)
                continue;
            if (pick == 0) {
                tag1 = slot.tag;
                fval = slot.value;
                break;
            }
            --pick;
        }

        present = 0;
        for (std::size_t index = 0; index < kTimeSlotsRead; ++index) {
            if (def->time_slots[index].access != 0)
                ++present;
        }
        pick = present > 1 ? rng.range(0, present - 1) : 0;
        for (std::size_t index = 0; index < kTimeSlotsRead; ++index) {
            const gl::assets::SpecialSlot &slot = def->time_slots[index];
            if (slot.access == 0)
                continue;
            if (pick == 0) {
                tag2 = slot.tag;
                fval *= slot.value;
                break;
            }
            --pick;
        }

        const gl::assets::SpecialFlags flags = tag1 | tag2;
        int value = static_cast<int>(fval * static_cast<float>(loot_level));
        int half = value >> 1;
        if (fval < 0.0f) {
            if (value >= 0)
                value = -1;
            if (half >= 0)
                half = -1;
            value = rng.range(half, value);
            if ((flags & kRelativeTag) == 0) {
                if (value == -1)
                    value -= rng.range(0, 1);
            } else {
                if (value > -3)
                    value -= rng.range(0, 3);
            }
        } else {
            if (value < 1)
                value = 1;
            if (half < 1)
                half = 1;
            value = rng.range(half, value);
            if ((flags & kRelativeTag) == 0) {
                if (value == 1)
                    value += rng.range(0, 1);
            } else {
                if (value < 3)
                    value += rng.range(0, 3);
            }
        }

        mods.push_back(gl::assets::SpecialMod{id, value, flags});
    }
    return mods;
}
} // namespace gl::game
