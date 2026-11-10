#include "gl/assets/item_catalog.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/specials.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/npc_loot.hpp"
#include "gl/game/person.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cerrno>
#include <compare>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#include <float.h>
#endif

namespace {

struct FpuControlWord {
    bool available = false;
    std::uint16_t value = 0;
    const char *source = "x87 control word is not available on this architecture";
};

FpuControlWord read_fpu_control_word() {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#if defined(_MSC_VER)
    const unsigned int word = _controlfp(0, 0);
    return {true, static_cast<std::uint16_t>(word & 0xffffu), "MSVC _controlfp"};
#else
    std::uint16_t word = 0;
    __asm__ __volatile__("fnstcw %0" : "=m"(word));
    return {true, word, "x87 fnstcw"};
#endif
#else
    return {};
#endif
}

bool fpu_reference_default() {
    const FpuControlWord word = read_fpu_control_word();
    return word.available && word.value == 0x027Fu;
}

struct CapturedMod {
    std::string person;
    std::uint32_t item_id = 0;
    std::int32_t count = 0;
    std::int32_t mod_id = 0;
    std::int32_t value = 0;
    std::uint32_t mod_flags = 0;
    std::uint32_t item_flags = 0;
    auto operator<=>(const CapturedMod &) const = default;
};

struct LiveCapture {
    std::string level;
    std::uint32_t seed = 0;
    std::int32_t loot_level = 0;
    std::vector<CapturedMod> mods;
};

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        text.remove_suffix(1);
    return text;
}

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t next = text.find(separator, start);
        if (next == std::string_view::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, next - start));
        start = next + 1;
    }
    return parts;
}

bool parse_int(std::string_view text, long long &value) {
    const std::string token(trim(text));
    if (token.empty())
        return false;
    errno = 0;
    char *end = nullptr;
    const long long parsed = std::strtoll(token.c_str(), &end, 0);
    if (errno != 0 || end == token.c_str() || *end != '\0')
        return false;
    value = parsed;
    return true;
}

bool parse_uint(std::string_view text, unsigned long long &value) {
    const std::string token(trim(text));
    if (token.empty() || token.front() == '-')
        return false;
    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(token.c_str(), &end, 0);
    if (errno != 0 || end == token.c_str() || *end != '\0')
        return false;
    value = parsed;
    return true;
}

bool parse_u32(std::string_view text, std::uint32_t &out) {
    unsigned long long value = 0;
    if (!parse_uint(text, value) || value > 0xffffffffull)
        return false;
    out = static_cast<std::uint32_t>(value);
    return true;
}

bool parse_i32(std::string_view text, std::int32_t &out) {
    long long value = 0;
    if (!parse_int(text, value) || value < -2147483648ll || value > 2147483647ll)
        return false;
    out = static_cast<std::int32_t>(value);
    return true;
}

bool parse_live_capture(std::string_view text, LiveCapture &capture, std::string &error) {
    capture = LiveCapture{};
    bool have_level = false;
    bool have_seed = false;
    bool have_loot_level = false;
    bool have_format = false;

    std::size_t line_number = 0;
    std::size_t offset = 0;
    if (text.substr(0, 3) == "\xEF\xBB\xBF")
        offset = 3;
    while (offset <= text.size()) {
        const std::size_t end = text.find('\n', offset);
        std::string_view line =
            end == std::string_view::npos ? text.substr(offset) : text.substr(offset, end - offset);
        offset = end == std::string_view::npos ? text.size() + 1 : end + 1;
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#')
            continue;

        const std::size_t equals = line.find('=');
        const std::size_t pipe = line.find('|');
        if (equals != std::string_view::npos && (pipe == std::string_view::npos || equals < pipe)) {
            const std::string_view key = trim(line.substr(0, equals));
            const std::string_view value = trim(line.substr(equals + 1));
            if (key == "format") {
                if (value != "gl-specials-live-1") {
                    error = "line " + std::to_string(line_number) + ": unsupported format '" +
                            std::string(value) + "'";
                    return false;
                }
                have_format = true;
            } else if (key == "level") {
                if (value.empty()) {
                    error = "line " + std::to_string(line_number) + ": empty level";
                    return false;
                }
                capture.level = std::string(value);
                have_level = true;
            } else if (key == "seed") {
                if (!parse_u32(value, capture.seed)) {
                    error = "line " + std::to_string(line_number) + ": invalid seed";
                    return false;
                }
                have_seed = true;
            } else if (key == "loot_level") {
                if (!parse_i32(value, capture.loot_level) || capture.loot_level <= 0) {
                    error = "line " + std::to_string(line_number) + ": invalid loot_level";
                    return false;
                }
                have_loot_level = true;
            } else {
                error = "line " + std::to_string(line_number) + ": unknown key '" +
                        std::string(key) + "'";
                return false;
            }
            continue;
        }

        const std::vector<std::string_view> fields = split(line, '|');
        if (fields.size() != 7) {
            error = "line " + std::to_string(line_number) + ": expected 7 fields, got " +
                    std::to_string(fields.size());
            return false;
        }
        CapturedMod record;
        record.person = std::string(trim(fields[0]));
        if (record.person.empty()) {
            error = "line " + std::to_string(line_number) + ": empty person";
            return false;
        }
        if (!parse_u32(fields[1], record.item_id) || record.item_id == 0) {
            error = "line " + std::to_string(line_number) + ": invalid item_id";
            return false;
        }
        if (!parse_i32(fields[2], record.count) || record.count <= 0) {
            error = "line " + std::to_string(line_number) + ": invalid count";
            return false;
        }
        if (!parse_i32(fields[3], record.mod_id) || record.mod_id < 0) {
            error = "line " + std::to_string(line_number) + ": invalid mod_id";
            return false;
        }
        if (!parse_i32(fields[4], record.value)) {
            error = "line " + std::to_string(line_number) + ": invalid value";
            return false;
        }
        if (!parse_u32(fields[5], record.mod_flags)) {
            error = "line " + std::to_string(line_number) + ": invalid mod_flags";
            return false;
        }
        if (!parse_u32(fields[6], record.item_flags)) {
            error = "line " + std::to_string(line_number) + ": invalid item_flags";
            return false;
        }
        capture.mods.push_back(std::move(record));
    }

    if (!have_level || !have_seed || !have_loot_level) {
        std::string missing;
        if (!have_level)
            missing += " level";
        if (!have_seed)
            missing += " seed";
        if (!have_loot_level)
            missing += " loot_level";
        error = "missing required header key(s):" + missing;
        return false;
    }
    if (!have_format)
        error = "warning: capture has no format=gl-specials-live-1 header";
    return true;
}

std::string live_capture_path() {
    const char *path = std::getenv("GL_SPECIALS_LIVE");
    return path == nullptr ? std::string{} : std::string(path);
}

std::string live_capture_unavailable_reason() {
    if (std::getenv("GL_GAME_DIR") == nullptr)
        return "GL_GAME_DIR is not set";
    const std::string path = live_capture_path();
    if (path.empty())
        return "GL_SPECIALS_LIVE is not set";
    std::error_code error;
    if (!std::filesystem::exists(path, error) || error)
        return "capture file not found: " + path;
    return {};
}

bool live_capture_available() { return live_capture_unavailable_reason().empty(); }

struct OracleComparison {
    std::size_t captured = 0;
    std::size_t port = 0;
    std::size_t matched = 0;
    std::size_t mismatched = 0;
    std::vector<std::string> details;
};

std::string describe(const CapturedMod &record) {
    std::ostringstream out;
    out << record.person << " item=" << record.item_id << " count=" << record.count
        << " mod=" << record.mod_id << " value=" << record.value << " mod_flags=0x" << std::hex
        << record.mod_flags << " item_flags=0x" << record.item_flags << std::dec;
    return out.str();
}

OracleComparison compare_records(const std::multiset<CapturedMod> &captured,
                                 const std::multiset<CapturedMod> &port) {
    OracleComparison result;
    result.captured = captured.size();
    result.port = port.size();

    std::set<CapturedMod> keys;
    keys.insert(captured.begin(), captured.end());
    keys.insert(port.begin(), port.end());
    for (const CapturedMod &key : keys) {
        const std::size_t live_count = captured.count(key);
        const std::size_t port_count = port.count(key);
        result.matched += std::min(live_count, port_count);
        if (live_count != port_count) {
            result.mismatched +=
                (live_count > port_count ? live_count - port_count : port_count - live_count);
            if (result.details.size() < 8) {
                std::ostringstream detail;
                detail << describe(key) << " captured=" << live_count << " port=" << port_count;
                result.details.push_back(detail.str());
            }
        }
    }
    return result;
}

gl::game::InventoryRuntime::ItemTemplateLookup make_lookup(const gl::assets::ItemCatalog &catalog) {
    return [&catalog](std::uint32_t item_id) -> std::optional<gl::game::ItemTemplate> {
        const gl::assets::ItemCatalogEntry *entry = catalog.find(item_id);
        if (entry == nullptr)
            return std::nullopt;
        return gl::game::ItemTemplate{static_cast<std::uint32_t>(entry->item.type),
                                      entry->item.flags, entry->item.unknown_b[1],
                                      entry->item.max_stack,
                                      static_cast<std::uint32_t>(entry->item.effects.size())};
    };
}

bool build_port_records(const LiveCapture &capture, std::multiset<CapturedMod> &records,
                        std::string &error) {
    const char *game_dir = std::getenv("GL_GAME_DIR");
    if (game_dir == nullptr) {
        error = "GL_GAME_DIR is not set";
        return false;
    }
    auto vfs = gl::assets::GameVfs::mount(game_dir);
    if (!vfs) {
        error = "cannot mount game data: " + vfs.error().message;
        return false;
    }
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    if (!catalog) {
        error = "cannot load item catalog: " + catalog.error().message;
        return false;
    }
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    if (!names) {
        error = "cannot load item names: " + names.error().message;
        return false;
    }
    auto level = gl::game::load_level(vfs.value(), capture.level);
    if (!level) {
        error = "cannot load level '" + capture.level + "': " + level.error().message;
        return false;
    }
    auto specials = gl::assets::load_specials_scr(vfs.value());
    if (!specials) {
        error = "cannot load specials.scr: " + specials.error().message;
        return false;
    }

    const auto lookup = make_lookup(catalog.value());
    gl::game::MsvcRandom rng(capture.seed);
    std::map<std::string, std::optional<gl::assets::InvTable>, std::less<>> cache;
    for (const gl::game::PersonInstance &person : level->persons) {
        if (person.inventory.empty() || person.name == "Hero")
            continue;
        auto cached = cache.find(person.inventory);
        if (cached == cache.end()) {
            auto table = gl::assets::load_inv(vfs.value(), person.inventory);
            if (!table)
                cached = cache.emplace(person.inventory, std::nullopt).first;
            else
                cached = cache.emplace(person.inventory, std::move(table.value())).first;
        }
        if (!cached->second)
            continue;
        const gl::game::NpcLoot loot = gl::game::NpcLootGenerator::generate(
            *cached->second, capture.loot_level, names.value(), lookup, rng, &specials.value());
        for (const gl::game::LootItem &item : loot.main) {
            for (const gl::assets::SpecialMod &mod : item.mods) {
                records.insert(CapturedMod{person.name, item.item_id, item.count,
                                           static_cast<std::int32_t>(mod.id), mod.value, mod.flags,
                                           item.flags});
            }
        }
    }
    return true;
}

} // namespace

TEST_CASE("special gates: x87 control word for generation and price" *
          doctest::skip(!fpu_reference_default())) {
    const FpuControlWord word = read_fpu_control_word();
    REQUIRE(word.available);
    std::ostringstream message;
    message << "x87 CW=0x" << std::hex << word.value << std::dec << " source=" << word.source
            << "; port specials generation and price compute in double/SSE2, so this validates "
               "the 0x027F reference environment assumption";
    MESSAGE(message.str());
    CHECK((word.value & 0x0300u) == 0x0200u);
    CHECK((word.value & 0x0C00u) == 0x0000u);
    CHECK(word.value == 0x027Fu);
}

TEST_CASE("special gates: live NPC-mod oracle" * doctest::skip(!live_capture_available())) {
    const std::string path = live_capture_path();
    std::ifstream file(path, std::ios::binary);
    REQUIRE_MESSAGE(file.good(), "cannot open capture: " << path);
    std::ostringstream buffer;
    buffer << file.rdbuf();

    LiveCapture capture;
    std::string error;
    REQUIRE_MESSAGE(parse_live_capture(buffer.str(), capture, error), error);
    MESSAGE("capture " << path << ": level=" << capture.level << " seed=" << capture.seed
                       << " loot_level=" << capture.loot_level
                       << " records=" << capture.mods.size());
    REQUIRE_MESSAGE(!capture.mods.empty(),
                    "capture contains no NPC mod records; refusing to fake a pass");

    std::multiset<CapturedMod> port;
    REQUIRE_MESSAGE(build_port_records(capture, port, error), error);

    const OracleComparison comparison =
        compare_records(std::multiset<CapturedMod>(capture.mods.begin(), capture.mods.end()), port);
    MESSAGE("oracle compare: captured=" << comparison.captured << " port=" << comparison.port
                                        << " matched=" << comparison.matched
                                        << " mismatched=" << comparison.mismatched);
    for (const std::string &detail : comparison.details)
        MESSAGE("mismatch: " << detail);
    CHECK(comparison.mismatched == 0u);
    CHECK(comparison.matched == comparison.captured);
    CHECK(comparison.captured == comparison.port);
}

TEST_CASE("special gates: live capture availability") {
    const FpuControlWord word = read_fpu_control_word();
    if (word.available) {
        std::ostringstream out;
        out << "x87 CW=0x" << std::hex << word.value << std::dec << " source=" << word.source
            << " reference=0x027f match=" << (word.value == 0x027Fu ? "yes" : "no");
        if (word.value != 0x027Fu)
            out << "; FPU-CW gate OPEN (expected 0x027F PC=53/RC=nearest; live original capture "
                   "required)";
        MESSAGE(out.str());
    } else {
        MESSAGE("FPU-CW gate skipped: " << word.source);
    }

    const std::string reason = live_capture_unavailable_reason();
    if (reason.empty()) {
        MESSAGE("live NPC-mod oracle available: " << live_capture_path());
    } else {
        MESSAGE("live NPC-mod oracle skipped: " << reason);
    }
}

TEST_CASE("special gates: capture format parser") {
    constexpr std::string_view text = "# gl-specials-live-capture v1\r\n"
                                      "format=gl-specials-live-1\r\n"
                                      "level=l1_1\r\n"
                                      "seed=1\r\n"
                                      "loot_level=1\r\n"
                                      "L1_2.P404_Sister_Of_Mercy2|8|1|25|-1|0x81|0x204\r\n";
    LiveCapture capture;
    std::string error;
    REQUIRE_MESSAGE(parse_live_capture(text, capture, error), error);
    CHECK(capture.level == "l1_1");
    CHECK(capture.seed == 1u);
    CHECK(capture.loot_level == 1);
    REQUIRE(capture.mods.size() == 1u);
    CHECK(capture.mods[0].person == "L1_2.P404_Sister_Of_Mercy2");
    CHECK(capture.mods[0].item_id == 8u);
    CHECK(capture.mods[0].count == 1);
    CHECK(capture.mods[0].mod_id == 25);
    CHECK(capture.mods[0].value == -1);
    CHECK(capture.mods[0].mod_flags == 0x81u);
    CHECK(capture.mods[0].item_flags == 0x204u);

    LiveCapture malformed;
    CHECK_FALSE(parse_live_capture("level=l1_1\nseed=1\n", malformed, error));
    CHECK(error.find("loot_level") != std::string::npos);
    CHECK_FALSE(parse_live_capture("level=l1_1\nseed=1\nloot_level=1\na|b|c\n", malformed, error));
    CHECK(error.find("expected 7 fields") != std::string::npos);
}

TEST_CASE("special gates: oracle comparison self-check") {
    const CapturedMod record{"L0.P744_oficer1", 256, 1, 20, 2, 0x41u, 0x304u};
    const std::multiset<CapturedMod> live{record};
    const std::multiset<CapturedMod> port{record};
    const OracleComparison same = compare_records(live, port);
    CHECK(same.mismatched == 0u);
    CHECK(same.matched == 1u);

    CapturedMod changed = record;
    changed.value = 3;
    const std::multiset<CapturedMod> different{changed};
    const OracleComparison mismatch = compare_records(live, different);
    CHECK(mismatch.mismatched == 2u);
    CHECK(mismatch.matched == 0u);
    CHECK(mismatch.details.size() == 2u);
}
