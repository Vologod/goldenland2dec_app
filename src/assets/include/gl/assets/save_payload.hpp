#pragma once
#include "gl/assets/result.hpp"
#include "gl/assets/save.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace gl::assets {
struct SaveBlockView {
    std::string tag;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

struct SaveInfo {
    std::string level_id;
    std::array<std::uint8_t, 0x20> time{};
    std::uint32_t field_218 = 0;
    std::int32_t party_level = 0;
    std::array<std::uint8_t, 8> field_26c8{};
};

struct SaveVar {
    std::uint32_t type = 0;
    std::string name;
    std::array<std::uint8_t, 8> value{};
    std::string text_value;
};

struct SaveEntity {
    std::uint32_t type = 0;
    std::vector<std::uint8_t> body;
};

struct SaveRginf {
    std::uint32_t latch = 0;
    std::array<std::uint32_t, 8> datetime{};
};

struct SaveTailEntry {
    std::string name;
    std::uint32_t raw_len = 0;
    std::vector<std::uint8_t> section;
    SaveRginf rginf;
    std::vector<std::string> person_names;
    std::vector<SaveEntity> entities;
};

struct SavePayload {
    SaveHeader header;
    std::vector<std::uint8_t> decompressed;
    std::vector<SaveBlockView> blocks;
    SaveInfo info;
    std::vector<SaveVar> vars;
    std::vector<SaveTailEntry> tail;
    std::optional<std::size_t> level_tail_index;
    std::vector<std::uint8_t> rginf;
    std::vector<std::uint8_t> ents;
};

Result<SavePayload> parse_save_payload(std::span<const std::uint8_t> file);
}
