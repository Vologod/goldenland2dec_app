#pragma once
#include "gl/assets/result.hpp"
#include "gl/assets/save.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

std::optional<double> save_var_number(const SaveVar &var);

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

struct SaveTribeEntry {
    std::uint32_t id = 0;
    std::string name;
};

struct SaveTribeTable {
    std::vector<SaveTribeEntry> entries;
    std::vector<std::uint8_t> matrix;

    std::size_t count() const { return entries.size(); }
    std::optional<std::size_t> index_of(std::uint32_t id) const;
    const SaveTribeEntry *find(std::uint32_t id) const;
    const SaveTribeEntry *find(std::string_view name) const;
    std::int32_t relation(std::uint32_t id_a, std::uint32_t id_b) const;
    std::span<const std::uint8_t> row(std::size_t index) const;
};

using SaveBlock58 = std::array<std::uint8_t, 0x58>;

struct SaveDiaryEntry {
    std::uint32_t key = 0;
    std::uint32_t declared_count = 0;
    std::vector<SaveBlock58> blocks;
};

struct SaveHistorySlot {
    std::string text;
    std::uint32_t id = 0;
};

struct SaveHistory {
    std::uint32_t head = 0;
    std::array<SaveHistorySlot, 40> slots;
    std::uint32_t tail = 0;
};

struct SavePayload {
    SaveHeader header;
    std::array<std::uint8_t, 0x68> header_raw{};
    std::vector<std::uint8_t> decompressed;
    std::vector<SaveBlockView> blocks;
    SaveInfo info;
    std::vector<SaveVar> vars;
    SaveTribeTable tribes;
    std::vector<SaveDiaryEntry> diary;
    std::vector<SaveBlock58> diary_sparse;
    SaveHistory history;
    std::vector<SaveTailEntry> tail;
    std::optional<std::size_t> level_tail_index;
    std::vector<std::uint8_t> rginf;
    std::vector<std::uint8_t> ents;
};

Result<SavePayload> parse_save_payload(std::span<const std::uint8_t> file);
Result<std::vector<std::uint8_t>> encode_save_payload(const SavePayload &payload);
Result<std::vector<std::uint8_t>> encode_vars_body(std::span<const SaveVar> vars);
Result<std::vector<std::uint8_t>> encode_tribe_body(const SaveTribeTable &table);
}
