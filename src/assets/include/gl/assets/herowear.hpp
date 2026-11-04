#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gl::assets {
struct HeroWearEntry {
    std::string tech;
    std::string category;
    std::string group;
    std::string base_set;
};

struct HeroWearCatalog {
    std::vector<HeroWearEntry> entries;
    const HeroWearEntry* find(std::string_view tech) const;
};

Result<HeroWearCatalog> parse_herowear(std::span<const std::uint8_t> text);
} // namespace gl::assets
