#pragma once
#include "gl/assets/result.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {
enum class PadAnimationType : std::uint32_t {
    rt_stay = 0x00000001,
    rt_fun = 0x00000002,
    tb_stay = 0x00000004,
    tb_fun = 0x00000008,
    tb_go = 0x00000010,
    rt_go = 0x00000020,
    cast = 0x00000040,
    suffer = 0x00000080,
    die = 0x00000100,
    ss_attack = 0x00000400,
    hits0 = 0x00010000,
    hits1 = 0x00020000,
    hits2 = 0x00040000,
    hits3 = 0x00080000,
};

inline constexpr std::array<PadAnimationType, 14> kPadAnimationTypes{
    PadAnimationType::rt_stay,   PadAnimationType::rt_fun, PadAnimationType::tb_stay,
    PadAnimationType::tb_fun,    PadAnimationType::tb_go,  PadAnimationType::rt_go,
    PadAnimationType::cast,      PadAnimationType::suffer, PadAnimationType::die,
    PadAnimationType::ss_attack, PadAnimationType::hits0,  PadAnimationType::hits1,
    PadAnimationType::hits2,     PadAnimationType::hits3,
};

struct PadCrop {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t w = 0;
    std::int16_t h = 0;
};

struct PadAnimation {
    PadAnimationType type = PadAnimationType::rt_stay;
    // Preserved verbatim, including zero or negative values; callers must guard
    // delay <= 0 before dividing simulation time (P4a Task 4 treats it as 1/skip).
    std::int32_t delay = 0;
    std::int32_t frames_per_row = 0;
    std::int32_t frame_width = 0;
    std::int32_t frame_height = 0;
    std::int32_t anchor_x = 0;
    std::int32_t anchor_y = 0;
    float movement_x = 0;
    float movement_y = 0;
    std::vector<std::vector<PadCrop>> crops;
    std::int32_t shadow_width = 0;
    std::int32_t shadow_height = 0;
    std::int32_t shadow_anchor_x = 0;
    std::int32_t shadow_anchor_y = 0;
    std::vector<std::vector<PadCrop>> shadow_crops;
};

struct PadDirectionRow {
    std::uint8_t row = 0;
    bool mirror = false;
};

inline constexpr std::array<std::uint8_t, 8> kPadPersonRows8{0, 1, 2, 3, 4, 3, 2, 1};
inline constexpr std::array<std::uint8_t, 8> kPadShadowRows8{0, 1, 2, 3, 4, 5, 6, 7};
inline constexpr std::array<std::uint8_t, 16> kPadPersonRows16{0, 1, 2, 3, 4, 5, 6, 7,
                                                               8, 7, 6, 5, 4, 3, 2, 1};
inline constexpr std::array<std::uint8_t, 16> kPadShadowRows16{0, 1, 2,  3,  4,  5,  6,  7,
                                                               8, 9, 10, 11, 12, 13, 14, 15};

// Parses the animation region [12, dataSize). After every header-mask animation
// is consumed, an optional tail table (u32 count + count*(i32,i32)) followed by
// complete repeated animation records is validated and ignored; bytes after
// dataSize are file trailers and are not inspected.
Result<std::vector<PadAnimation>> parse_pad(std::span<const std::uint8_t> data);
bool pad_is_go(PadAnimationType type);
// Rows are indexes into PadAnimation::crops and must always be bounds-checked
// against crops.size(). Standard person tables have 5 unique rows (8 directions)
// or 9 unique rows (16 directions for _go); non-go types use the 8-direction table.
PadDirectionRow pad_person_row(PadAnimationType type, std::uint8_t direction);
// Shadow tables have one row per direction (8 or 16) and are identity: shadows
// never mirror.
PadDirectionRow pad_shadow_row(PadAnimationType type, std::uint8_t direction);
} // namespace gl::assets
