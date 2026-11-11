#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_catalog.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/specials.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/npc_loot.hpp"
#include "gl/game/person.hpp"
#include "gl/game/special_engine.hpp"
#include "gl/game/trade_price.hpp"

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

#if (defined(__i386__) || defined(__x86_64__)) && !defined(_MSC_VER)
#define GL_TEST_X86_ROUNDING 1
#include <xmmintrin.h>
#endif

namespace {

constexpr std::uint16_t kReferenceControlWord = 0x027Fu;

using gl::assets::SpecialDatabase;
using gl::assets::SpecialDef;
using gl::assets::SpecialFlags;
using gl::assets::SpecialId;
using gl::assets::SpecialSlot;

#if defined(GL_TEST_X86_ROUNDING)

std::uint16_t read_x87_control_word() {
    std::uint16_t word = 0;
    __asm__ __volatile__("fnstcw %0" : "=m"(word));
    return word;
}

void write_x87_control_word(std::uint16_t word) {
    __asm__ __volatile__("fldcw %0" : : "m"(word));
}

class ScopedRoundingMode {
  public:
    ScopedRoundingMode(std::uint16_t control_word, std::uint32_t mxcsr_rounding)
        : old_control_word_(read_x87_control_word()), old_mxcsr_(_mm_getcsr()) {
        write_x87_control_word(control_word);
        _mm_setcsr((old_mxcsr_ & ~0x6000u) | mxcsr_rounding);
    }

    ~ScopedRoundingMode() {
        write_x87_control_word(old_control_word_);
        _mm_setcsr(old_mxcsr_);
    }

    ScopedRoundingMode(const ScopedRoundingMode &) = delete;
    ScopedRoundingMode &operator=(const ScopedRoundingMode &) = delete;

  private:
    std::uint16_t old_control_word_ = 0;
    std::uint32_t old_mxcsr_ = 0;
};

bool x86_rounding_available() { return true; }

#else

bool x86_rounding_available() { return false; }

#endif

struct LiveFpuControlWord {
    bool ok = false;
    std::uint16_t generation = 0;
    std::uint16_t price = 0;
    std::string source;
    std::string error;
};

struct CapturedItem {
    std::string person;
    std::uint32_t item_id = 0;
    std::int32_t count = 0;
    std::uint32_t item_flags = 0;
    std::uint32_t mod_count = 0;
    auto operator<=>(const CapturedItem &) const = default;
};

struct CapturedMod {
    std::string person;
    std::uint32_t item_id = 0;
    std::int32_t count = 0;
    std::uint32_t item_flags = 0;
    std::int32_t mod_id = 0;
    std::int32_t value = 0;
    std::uint32_t mod_flags = 0;
    auto operator<=>(const CapturedMod &) const = default;
};

struct LiveCapture {
    std::string level;
    std::uint32_t seed = 0;
    std::uint32_t rng_state = 0;
    std::int32_t loot_level = 0;
    std::vector<CapturedItem> items;
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

bool parse_u16(std::string_view text, std::uint16_t &out) {
    unsigned long long value = 0;
    if (!parse_uint(text, value) || value > 0xffffull)
        return false;
    out = static_cast<std::uint16_t>(value);
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
    bool have_rng_state = false;
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
                if (value != "gl-specials-live-2") {
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
            } else if (key == "rng_state") {
                if (!parse_u32(value, capture.rng_state)) {
                    error = "line " + std::to_string(line_number) + ": invalid rng_state";
                    return false;
                }
                have_rng_state = true;
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
        if (fields.empty()) {
            error = "line " + std::to_string(line_number) + ": empty row";
            return false;
        }
        const std::string_view kind = trim(fields[0]);
        if (kind == "item") {
            if (fields.size() != 6) {
                error = "line " + std::to_string(line_number) +
                        ": item row expects 6 fields, got " + std::to_string(fields.size());
                return false;
            }
            CapturedItem item;
            item.person = std::string(trim(fields[1]));
            if (item.person.empty() || !parse_u32(fields[2], item.item_id) || item.item_id == 0 ||
                !parse_i32(fields[3], item.count) || item.count <= 0 ||
                !parse_u32(fields[4], item.item_flags) || !parse_u32(fields[5], item.mod_count)) {
                error = "line " + std::to_string(line_number) + ": invalid item row";
                return false;
            }
            capture.items.push_back(std::move(item));
        } else if (kind == "mod") {
            if (fields.size() != 8) {
                error = "line " + std::to_string(line_number) + ": mod row expects 8 fields, got " +
                        std::to_string(fields.size());
                return false;
            }
            CapturedMod mod;
            mod.person = std::string(trim(fields[1]));
            if (mod.person.empty() || !parse_u32(fields[2], mod.item_id) || mod.item_id == 0 ||
                !parse_i32(fields[3], mod.count) || mod.count <= 0 ||
                !parse_u32(fields[4], mod.item_flags) || !parse_i32(fields[5], mod.mod_id) ||
                mod.mod_id < 0 || !parse_i32(fields[6], mod.value) ||
                !parse_u32(fields[7], mod.mod_flags)) {
                error = "line " + std::to_string(line_number) + ": invalid mod row";
                return false;
            }
            capture.mods.push_back(std::move(mod));
        } else {
            error = "line " + std::to_string(line_number) + ": unknown row kind '" +
                    std::string(kind) + "'";
            return false;
        }
    }

    if (!have_level || !have_rng_state || !have_loot_level) {
        std::string missing;
        if (!have_level)
            missing += " level";
        if (!have_rng_state)
            missing += " rng_state";
        if (!have_loot_level)
            missing += " loot_level";
        error = "missing required header key(s):" + missing;
        return false;
    }
    if (!have_seed)
        capture.seed = 0;
    if (!have_format)
        error = "warning: capture has no format=gl-specials-live-2 header";
    return true;
}

std::string env_value(const char *name) {
    const char *value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

bool set_env_value(const char *name, const std::string &value) {
#ifdef _WIN32
    return _putenv_s(name, value.c_str()) == 0;
#else
    return setenv(name, value.c_str(), 1) == 0;
#endif
}

void clear_env_value(const char *name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::string live_capture_path() { return env_value("GL_SPECIALS_LIVE"); }

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

LiveFpuControlWord load_live_fpu_control_word() {
    LiveFpuControlWord result;
    const std::string value = env_value("GL_SPECIALS_FPU_CW");
    if (value.empty()) {
        result.error = "GL_SPECIALS_FPU_CW is not set";
        return result;
    }

    std::uint16_t both = 0;
    if (parse_u16(value, both)) {
        result.ok = true;
        result.generation = both;
        result.price = both;
        result.source = "GL_SPECIALS_FPU_CW=" + value;
        return result;
    }

    std::ifstream file(value, std::ios::binary);
    if (!file.good()) {
        result.error = "cannot open GL_SPECIALS_FPU_CW file: " + value;
        return result;
    }
    bool have_generation = false;
    bool have_price = false;
    std::string line;
    while (std::getline(file, line)) {
        const std::string_view trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#')
            continue;
        const std::size_t equals = trimmed.find('=');
        if (equals == std::string_view::npos) {
            result.error = "invalid FPU-CW line: " + std::string(trimmed);
            return result;
        }
        const std::string_view key = trim(trimmed.substr(0, equals));
        const std::string_view content = trim(trimmed.substr(equals + 1));
        if (key == "generation") {
            if (!parse_u16(content, result.generation)) {
                result.error = "invalid generation CW: " + std::string(content);
                return result;
            }
            have_generation = true;
        } else if (key == "price") {
            if (!parse_u16(content, result.price)) {
                result.error = "invalid price CW: " + std::string(content);
                return result;
            }
            have_price = true;
        } else if (key == "source") {
            result.source = std::string(content);
        } else {
            result.error = "unknown FPU-CW key: " + std::string(key);
            return result;
        }
    }
    if (!have_generation || !have_price) {
        result.error = "FPU-CW file needs generation= and price= keys";
        return result;
    }
    result.ok = true;
    if (result.source.empty())
        result.source = value;
    return result;
}

bool live_fpu_configured() { return !env_value("GL_SPECIALS_FPU_CW").empty(); }

struct OracleComparison {
    std::size_t captured_items = 0;
    std::size_t port_items = 0;
    std::size_t matched_items = 0;
    std::size_t mismatched_items = 0;
    std::size_t captured_mods = 0;
    std::size_t port_mods = 0;
    std::size_t matched_mods = 0;
    std::size_t mismatched_mods = 0;
    std::vector<std::string> details;

    std::size_t mismatched() const { return mismatched_items + mismatched_mods; }
};

std::string describe(const CapturedItem &record) {
    std::ostringstream out;
    out << "item " << record.person << " item=" << record.item_id << " count=" << record.count
        << " item_flags=0x" << std::hex << record.item_flags << std::dec
        << " mod_count=" << record.mod_count;
    return out.str();
}

std::string describe(const CapturedMod &record) {
    std::ostringstream out;
    out << "mod " << record.person << " item=" << record.item_id << " count=" << record.count
        << " item_flags=0x" << std::hex << record.item_flags << " mod=" << std::dec << record.mod_id
        << " value=" << record.value << " mod_flags=0x" << std::hex << record.mod_flags << std::dec;
    return out.str();
}

template <typename Record>
void compare_records(const std::multiset<Record> &captured, const std::multiset<Record> &port,
                     std::size_t &matched, std::size_t &mismatched,
                     std::vector<std::string> &details) {
    std::set<Record> keys;
    keys.insert(captured.begin(), captured.end());
    keys.insert(port.begin(), port.end());
    for (const Record &key : keys) {
        const std::size_t captured_count = captured.count(key);
        const std::size_t port_count = port.count(key);
        matched += std::min(captured_count, port_count);
        if (captured_count != port_count) {
            mismatched += (captured_count > port_count ? captured_count - port_count
                                                       : port_count - captured_count);
            if (details.size() < 8) {
                std::ostringstream detail;
                detail << describe(key) << " captured=" << captured_count << " port=" << port_count;
                details.push_back(detail.str());
            }
        }
    }
}

OracleComparison compare_records(const std::multiset<CapturedItem> &captured_items,
                                 const std::multiset<CapturedItem> &port_items,
                                 const std::multiset<CapturedMod> &captured_mods,
                                 const std::multiset<CapturedMod> &port_mods) {
    OracleComparison result;
    result.captured_items = captured_items.size();
    result.port_items = port_items.size();
    result.captured_mods = captured_mods.size();
    result.port_mods = port_mods.size();
    compare_records(captured_items, port_items, result.matched_items, result.mismatched_items,
                    result.details);
    compare_records(captured_mods, port_mods, result.matched_mods, result.mismatched_mods,
                    result.details);
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

struct PortRun {
    std::multiset<CapturedItem> items;
    std::multiset<CapturedMod> mods;
    std::vector<std::string> order;
    std::size_t generated_persons = 0;
    std::size_t captured_persons = 0;
    std::size_t inherited_capture_items = 0;
    std::size_t inherited_capture_mods = 0;
    std::size_t inherited_port_items = 0;
};

bool item_has_inherited_mods(const gl::assets::ItemCatalog &catalog, std::uint32_t item_id) {
    const gl::assets::ItemCatalogEntry *entry = catalog.find(item_id);
    return entry != nullptr && !entry->item.effects.empty();
}

void filter_inherited_capture(LiveCapture &capture, const gl::assets::ItemCatalog &catalog,
                              PortRun &port) {
    port.inherited_capture_items =
        std::erase_if(capture.items, [&catalog](const CapturedItem &item) {
            return item_has_inherited_mods(catalog, item.item_id);
        });
    port.inherited_capture_mods =
        std::erase_if(capture.mods, [&catalog](const CapturedMod &mod) {
            return item_has_inherited_mods(catalog, mod.item_id);
        });
}

bool build_port_records(LiveCapture &capture, bool filter_to_captured, PortRun &port,
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
    filter_inherited_capture(capture, catalog.value(), port);
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

    std::set<std::string> captured_persons;
    for (const CapturedItem &item : capture.items)
        captured_persons.insert(item.person);
    for (const CapturedMod &mod : capture.mods)
        captured_persons.insert(mod.person);
    port.captured_persons = captured_persons.size();

    const auto lookup = make_lookup(catalog.value());
    gl::game::MsvcRandom rng(capture.rng_state);
    std::map<std::string, std::optional<gl::assets::InvTable>, std::less<>> cache;
    for (const gl::game::PersonInstance &person : level->persons) {
        if (person.inventory.empty() || person.name == "Hero")
            continue;
        ++port.generated_persons;
        port.order.push_back(person.name + "(" + person.inventory + ")");
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
        if (filter_to_captured && !captured_persons.contains(person.name))
            continue;
        for (const gl::game::LootItem &item : loot.main) {
            const auto item_template = lookup(item.item_id);
            if (item_template.has_value() && item_template->mod_count > 0) {
                ++port.inherited_port_items;
                continue;
            }
            port.items.insert(CapturedItem{person.name, item.item_id, item.count, item.flags,
                                           item.mod_count});
            for (const gl::assets::SpecialMod &mod : item.mods) {
                port.mods.insert(CapturedMod{person.name, item.item_id, item.count, item.flags,
                                             static_cast<std::int32_t>(mod.id), mod.value,
                                             mod.flags});
            }
        }
    }
    return true;
}

SpecialDef make_special_def(SpecialId id, float value, SpecialFlags value_tag,
                            SpecialFlags time_tag) {
    SpecialDef def;
    def.id = id;
    def.value_slots[0] = SpecialSlot{value_tag, 1, value, 0.0f};
    def.time_slots[0] = SpecialSlot{time_tag, 1, 1.0f, 0.0f};
    return def;
}

SpecialDatabase make_invariance_db() {
    SpecialDatabase db;
    db.defs = {
        make_special_def(0, 0.5f, 0x40, 1),  make_special_def(1, 0.1f, 0x40, 1),
        make_special_def(2, 0.25f, 0x40, 1), make_special_def(3, 1.0f, 0x40, 1),
        make_special_def(4, -0.5f, 0x80, 1), make_special_def(5, 0.3f, 0x80, 1),
    };
    db.item_classes[gl::assets::ItemType::sword] = {0, 1, 2, 3, 4, 5};
    return db;
}

std::vector<gl::assets::SpecialMod> run_specials(const SpecialDatabase &db, int nspec) {
    gl::game::MsvcRandom rng(1);
    return gl::game::SpecialEngine::apply(db, gl::assets::ItemType::sword, nspec, 7, rng);
}

bool same_mods(const std::vector<gl::assets::SpecialMod> &left,
               const std::vector<gl::assets::SpecialMod> &right) {
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].id != right[index].id || left[index].value != right[index].value ||
            left[index].flags != right[index].flags)
            return false;
    }
    return true;
}

bool same_price(const std::optional<gl::game::TradePrice> &left,
                const std::optional<gl::game::TradePrice> &right) {
    if (left.has_value() != right.has_value())
        return false;
    if (!left)
        return true;
    return left->item_id == right->item_id && left->buy == right->buy &&
           left->sell == right->sell && left->count == right->count;
}

std::vector<std::optional<gl::game::TradePrice>>
run_prices(const std::vector<gl::game::TradePriceInput> &inputs) {
    std::vector<std::optional<gl::game::TradePrice>> prices;
    prices.reserve(inputs.size());
    for (const gl::game::TradePriceInput &input : inputs)
        prices.push_back(gl::game::compute_trade_price(input));
    return prices;
}

} // namespace

TEST_CASE("special gates: live original FPU control word" * doctest::skip(!live_fpu_configured())) {
    const LiveFpuControlWord captured = load_live_fpu_control_word();
    REQUIRE_MESSAGE(captured.ok, captured.error);
    std::ostringstream message;
    message << "live FPU-CW source=" << captured.source << " generation=0x" << std::hex
            << captured.generation << " price=0x" << captured.price << std::dec;
    MESSAGE(message.str());
    CHECK(captured.generation == kReferenceControlWord);
    CHECK(captured.price == kReferenceControlWord);
    CHECK((captured.generation & 0x0300u) == 0x0200u);
    CHECK((captured.generation & 0x0C00u) == 0x0000u);
    CHECK((captured.price & 0x0300u) == 0x0200u);
    CHECK((captured.price & 0x0C00u) == 0x0000u);
}

TEST_CASE("special gates: rounding invariance (diagnostic; default MXCSR nearest)" *
          doctest::skip(!x86_rounding_available())) {
#if defined(GL_TEST_X86_ROUNDING)
    const SpecialDatabase db = make_invariance_db();
    const std::vector<gl::assets::SpecialMod> special_reference = run_specials(db, 3);
    const std::vector<gl::assets::SpecialMod> special_all = run_specials(db, 8);

    const std::vector<gl::game::TradePriceInput> price_inputs = {
        {953, 500, 100, 0x8000000Cu, 0, 1, 14, true},
        {256, 40, 37, 0x304u, 0, 3, 7, true},
        {777, 12345, 63, 0u, 0, 17, 3, true},
    };
    const std::vector<std::optional<gl::game::TradePrice>> price_reference =
        run_prices(price_inputs);

    struct Mode {
        const char *name;
        std::uint16_t control_word;
        std::uint32_t mxcsr_rounding;
    };
    const Mode x87_modes[] = {
        {"x87 PC53/nearest", 0x027Fu, 0x0000u}, {"x87 PC64/nearest", 0x037Fu, 0x0000u},
        {"x87 PC53/down", 0x067Fu, 0x0000u},    {"x87 PC53/up", 0x0A7Fu, 0x0000u},
        {"x87 PC53/trunc", 0x0E7Fu, 0x0000u},
    };
    for (const Mode &mode : x87_modes) {
        INFO(mode.name);
        ScopedRoundingMode scoped(mode.control_word, mode.mxcsr_rounding);
        CHECK((read_x87_control_word() & 0x0F00u) == (mode.control_word & 0x0F00u));
        CHECK((_mm_getcsr() & 0x6000u) == mode.mxcsr_rounding);
        CHECK(same_mods(run_specials(db, 3), special_reference));
        CHECK(same_mods(run_specials(db, 8), special_all));
        const auto prices = run_prices(price_inputs);
        REQUIRE(prices.size() == price_reference.size());
        for (std::size_t index = 0; index < prices.size(); ++index)
            CHECK(same_price(prices[index], price_reference[index]));
    }

    const Mode mxcsr_modes[] = {
        {"x87 PC53/nearest, MXCSR nearest", 0x027Fu, 0x0000u},
        {"x87 PC53/nearest, MXCSR down", 0x027Fu, 0x2000u},
        {"x87 PC53/nearest, MXCSR up", 0x027Fu, 0x4000u},
        {"x87 PC53/nearest, MXCSR trunc", 0x027Fu, 0x6000u},
    };
    std::size_t price_sensitive_modes = 0;
    for (const Mode &mode : mxcsr_modes) {
        INFO(mode.name);
        ScopedRoundingMode scoped(mode.control_word, mode.mxcsr_rounding);
        CHECK((read_x87_control_word() & 0x0F00u) == (mode.control_word & 0x0F00u));
        CHECK((_mm_getcsr() & 0x6000u) == mode.mxcsr_rounding);
        CHECK(same_mods(run_specials(db, 3), special_reference));
        CHECK(same_mods(run_specials(db, 8), special_all));
        const auto prices = run_prices(price_inputs);
        REQUIRE(prices.size() == price_reference.size());
        if (mode.mxcsr_rounding == 0x0000u) {
            for (std::size_t index = 0; index < prices.size(); ++index)
                CHECK(same_price(prices[index], price_reference[index]));
        } else {
            bool changed = false;
            for (std::size_t index = 0; index < prices.size(); ++index)
                changed = changed || !same_price(prices[index], price_reference[index]);
            if (changed)
                ++price_sensitive_modes;
        }
    }
    MESSAGE("TradePrice MXCSR-sensitive non-nearest modes: "
            << price_sensitive_modes
            << "/3; the port must keep the default MXCSR rounding (nearest)");
#endif
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
                       << " rng_state=0x" << std::hex << capture.rng_state << std::dec
                       << " loot_level=" << capture.loot_level << " items=" << capture.items.size()
                       << " mods=" << capture.mods.size());
    REQUIRE_MESSAGE(!capture.items.empty(), "capture contains no NPC item rows");
    REQUIRE_MESSAGE(!capture.mods.empty(), "capture contains no NPC mod records");

    PortRun port;
    REQUIRE_MESSAGE(build_port_records(capture, true, port, error), error);
    std::ostringstream order;
    for (const std::string &name : port.order)
        order << name << " ";
    MESSAGE("port order: " << order.str());
    if (port.inherited_capture_items != 0 || port.inherited_capture_mods != 0 ||
        port.inherited_port_items != 0) {
        MESSAGE("oracle scope: skipped inherited ITM-effect records (capture items="
                << port.inherited_capture_items << ", capture mods=" << port.inherited_capture_mods
                << ", port items=" << port.inherited_port_items
                << "); prototype effect semantics are not modelled");
    }
    if (port.captured_persons < port.generated_persons) {
        MESSAGE("partial capture: compared "
                << port.captured_persons << "/" << port.generated_persons
                << " generated persons; this is reduced-scope evidence, not full parity");
    }

    const OracleComparison comparison = compare_records(
        std::multiset<CapturedItem>(capture.items.begin(), capture.items.end()), port.items,
        std::multiset<CapturedMod>(capture.mods.begin(), capture.mods.end()), port.mods);
    MESSAGE("oracle compare: items captured="
            << comparison.captured_items << " port=" << comparison.port_items << " matched="
            << comparison.matched_items << " mismatched=" << comparison.mismatched_items
            << "; mods captured=" << comparison.captured_mods << " port=" << comparison.port_mods
            << " matched=" << comparison.matched_mods
            << " mismatched=" << comparison.mismatched_mods);
    for (const std::string &detail : comparison.details)
        MESSAGE("mismatch: " << detail);
    CHECK(comparison.mismatched_items == 0u);
    CHECK(comparison.mismatched_mods == 0u);
    CHECK(comparison.matched_items == comparison.captured_items);
    CHECK(comparison.matched_mods == comparison.captured_mods);
    CHECK(comparison.captured_items == comparison.port_items);
    CHECK(comparison.captured_mods == comparison.port_mods);
}

TEST_CASE("special gates: live capture availability") {
#if defined(GL_TEST_X86_ROUNDING)
    std::ostringstream host_fpu;
    host_fpu << "host x87 CW=0x" << std::hex << read_x87_control_word() << std::dec
             << " (diagnostic only; not the reference capture)";
    MESSAGE(host_fpu.str());
#else
    MESSAGE("x87 diagnostics skipped: non-x86 target uses IEEE double");
#endif

    const LiveFpuControlWord fpu = load_live_fpu_control_word();
    if (fpu.ok) {
        std::ostringstream message;
        message << "live FPU-CW available: generation=0x" << std::hex << fpu.generation
                << " price=0x" << fpu.price << std::dec;
        MESSAGE(message.str());
    } else {
        MESSAGE("live FPU-CW gate skipped: " << fpu.error);
    }

    const std::string reason = live_capture_unavailable_reason();
    if (reason.empty()) {
        MESSAGE("live NPC-mod oracle available: " << live_capture_path());
    } else {
        MESSAGE("live NPC-mod oracle skipped: " << reason);
    }
}

TEST_CASE("special gates: oracle plumbing self-check" * doctest::skip(!game_dir_available())) {
    LiveCapture session;
    session.level = "l1_1";
    session.rng_state = 1;
    session.loot_level = 1;

    PortRun all;
    std::string error;
    REQUIRE_MESSAGE(build_port_records(session, false, all, error), error);
    std::ostringstream order;
    for (const std::string &name : all.order)
        order << name << " ";
    MESSAGE("port order (synthetic): " << order.str());
    REQUIRE_MESSAGE(!all.items.empty(), "port generated no main-container items");
    REQUIRE_MESSAGE(!all.mods.empty(), "port generated no main-container mods");
    CHECK(std::any_of(all.items.begin(), all.items.end(),
                      [](const CapturedItem &item) { return item.mod_count == 0; }));
    CHECK(std::any_of(all.items.begin(), all.items.end(),
                      [](const CapturedItem &item) { return item.mod_count != 0; }));

    const std::string person = all.mods.begin()->person;
    LiveCapture partial;
    partial.level = session.level;
    partial.rng_state = session.rng_state;
    partial.loot_level = session.loot_level;
    for (const CapturedItem &item : all.items) {
        if (item.person == person)
            partial.items.push_back(item);
    }
    for (const CapturedMod &mod : all.mods) {
        if (mod.person == person)
            partial.mods.push_back(mod);
    }
    REQUIRE(!partial.items.empty());
    REQUIRE(!partial.mods.empty());

    PortRun filtered;
    REQUIRE_MESSAGE(build_port_records(partial, true, filtered, error), error);
    CHECK(filtered.captured_persons == 1u);
    const OracleComparison matched = compare_records(
        std::multiset<CapturedItem>(partial.items.begin(), partial.items.end()), filtered.items,
        std::multiset<CapturedMod>(partial.mods.begin(), partial.mods.end()), filtered.mods);
    CHECK(matched.mismatched() == 0u);
    CHECK(matched.captured_items == matched.port_items);
    CHECK(matched.captured_mods == matched.port_mods);

    std::vector<CapturedItem> extra_items = partial.items;
    extra_items.push_back(CapturedItem{person, 0xFFFFFFF0u, 1, 0x200u, 0});
    const OracleComparison extra_item = compare_records(
        std::multiset<CapturedItem>(extra_items.begin(), extra_items.end()), filtered.items,
        std::multiset<CapturedMod>(partial.mods.begin(), partial.mods.end()), filtered.mods);
    CHECK(extra_item.mismatched_items == 1u);

    std::vector<CapturedMod> extra_mods = partial.mods;
    extra_mods.push_back(CapturedMod{person, 0xFFFFFFF0u, 1, 0x200u, 77, 1, 0x41u});
    const OracleComparison extra_mod = compare_records(
        std::multiset<CapturedItem>(partial.items.begin(), partial.items.end()), filtered.items,
        std::multiset<CapturedMod>(extra_mods.begin(), extra_mods.end()), filtered.mods);
    CHECK(extra_mod.mismatched_mods == 1u);
    MESSAGE("synthetic plumbing self-check only; not live capture evidence");
}

TEST_CASE("special gates: inherited ITM-effect rows are excluded from comparison" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    const gl::assets::ItemCatalogEntry *effects_entry = catalog.value().find(411);
    REQUIRE(effects_entry != nullptr);
    REQUIRE(!effects_entry->item.effects.empty());

    const CapturedItem inherited{"L0.Prototype", 411, 1, 0x0Cu, 1};
    const CapturedMod inherited_mod{"L0.Prototype", 411, 1, 0x0Cu, 7, 1, 0x80u};
    const CapturedItem generated{"L1_2.P404_Sister_Of_Mercy2", 8, 1, 0x204u, 1};
    const CapturedMod generated_mod{"L1_2.P404_Sister_Of_Mercy2", 8, 1, 0x204u, 25, -1, 0x81u};

    LiveCapture capture;
    capture.level = "l1_1";
    capture.rng_state = 1;
    capture.loot_level = 1;
    capture.items = {inherited, generated};
    capture.mods = {inherited_mod, generated_mod};

    PortRun port;
    filter_inherited_capture(capture, catalog.value(), port);
    CHECK(port.inherited_capture_items == 1u);
    CHECK(port.inherited_capture_mods == 1u);
    REQUIRE(capture.items.size() == 1u);
    CHECK(capture.items[0] == generated);
    REQUIRE(capture.mods.size() == 1u);
    CHECK(capture.mods[0] == generated_mod);

    const OracleComparison comparison = compare_records(
        std::multiset<CapturedItem>(capture.items.begin(), capture.items.end()),
        std::multiset<CapturedItem>{generated},
        std::multiset<CapturedMod>(capture.mods.begin(), capture.mods.end()),
        std::multiset<CapturedMod>{generated_mod});
    CHECK(comparison.mismatched() == 0u);
    CHECK(comparison.captured_items == 1u);
    CHECK(comparison.captured_mods == 1u);
}

TEST_CASE("special gates: FPU-CW file fallback") {
    const std::string old = env_value("GL_SPECIALS_FPU_CW");
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "gl_special_fpu_cw_test.txt";
    std::error_code remove_error;
    std::filesystem::remove(path, remove_error);
    {
        std::ofstream file(path, std::ios::binary);
        REQUIRE(file.good());
        file << "# synthetic unit fixture\n"
                "source=unit-file\n"
                "generation=0x027F\n"
                "price=0x037F\n";
    }

    REQUIRE(set_env_value("GL_SPECIALS_FPU_CW", path.string()));
    const LiveFpuControlWord from_file = load_live_fpu_control_word();
    CHECK(from_file.ok);
    CHECK(from_file.generation == 0x027Fu);
    CHECK(from_file.price == 0x037Fu);
    CHECK(from_file.source == "unit-file");

    REQUIRE(set_env_value("GL_SPECIALS_FPU_CW", "0x027F"));
    const LiveFpuControlWord single = load_live_fpu_control_word();
    CHECK(single.ok);
    CHECK(single.generation == 0x027Fu);
    CHECK(single.price == 0x027Fu);

    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        REQUIRE(file.good());
        file << "generation=0x027F\n";
    }
    REQUIRE(set_env_value("GL_SPECIALS_FPU_CW", path.string()));
    const LiveFpuControlWord missing_price = load_live_fpu_control_word();
    CHECK_FALSE(missing_price.ok);
    CHECK(missing_price.error.find("price") != std::string::npos);

    std::filesystem::remove(path, remove_error);
    if (old.empty())
        clear_env_value("GL_SPECIALS_FPU_CW");
    else
        set_env_value("GL_SPECIALS_FPU_CW", old);
}

TEST_CASE("special gates: capture format parser") {
    constexpr std::string_view text = "# gl-specials-live-capture v2\r\n"
                                      "format=gl-specials-live-2\r\n"
                                      "level=l1_1\r\n"
                                      "seed=1\r\n"
                                      "rng_state=0x12345678\r\n"
                                      "loot_level=1\r\n"
                                      "item|L1_2.P404_Sister_Of_Mercy2|8|1|0x204|1\r\n"
                                      "mod|L1_2.P404_Sister_Of_Mercy2|8|1|0x204|25|-1|0x81\r\n";
    LiveCapture capture;
    std::string error;
    REQUIRE_MESSAGE(parse_live_capture(text, capture, error), error);
    CHECK(capture.level == "l1_1");
    CHECK(capture.seed == 1u);
    CHECK(capture.rng_state == 0x12345678u);
    CHECK(capture.loot_level == 1);
    REQUIRE(capture.items.size() == 1u);
    CHECK(capture.items[0].person == "L1_2.P404_Sister_Of_Mercy2");
    CHECK(capture.items[0].item_id == 8u);
    CHECK(capture.items[0].count == 1);
    CHECK(capture.items[0].item_flags == 0x204u);
    CHECK(capture.items[0].mod_count == 1u);
    REQUIRE(capture.mods.size() == 1u);
    CHECK(capture.mods[0].mod_id == 25);
    CHECK(capture.mods[0].value == -1);
    CHECK(capture.mods[0].mod_flags == 0x81u);

    LiveCapture malformed;
    CHECK_FALSE(parse_live_capture("level=l1_1\nseed=1\nrng_state=1\n", malformed, error));
    CHECK(error.find("loot_level") != std::string::npos);
    CHECK_FALSE(parse_live_capture("level=l1_1\nrng_state=1\nloot_level=1\nitem|P|1|1|0\n",
                                   malformed, error));
    CHECK(error.find("item row expects 6 fields") != std::string::npos);
    CHECK_FALSE(
        parse_live_capture("level=l1_1\nrng_state=1\nloot_level=1\nother|P|1\n", malformed, error));
    CHECK(error.find("unknown row kind") != std::string::npos);
}

TEST_CASE("special gates: oracle comparison self-check") {
    const CapturedItem item{"L0.P744_oficer1", 256, 1, 0x304u, 1};
    const CapturedMod mod{"L0.P744_oficer1", 256, 1, 0x304u, 20, 2, 0x41u};
    const std::multiset<CapturedItem> live_items{item};
    const std::multiset<CapturedItem> port_items{item};
    const std::multiset<CapturedMod> live_mods{mod};
    const std::multiset<CapturedMod> port_mods{mod};
    const OracleComparison same = compare_records(live_items, port_items, live_mods, port_mods);
    CHECK(same.mismatched() == 0u);
    CHECK(same.matched_items == 1u);
    CHECK(same.matched_mods == 1u);

    CapturedItem changed_item = item;
    changed_item.item_flags = 0x104u;
    const std::multiset<CapturedItem> different_items{changed_item};
    const OracleComparison item_mismatch =
        compare_records(live_items, different_items, live_mods, port_mods);
    CHECK(item_mismatch.mismatched_items == 2u);
    CHECK(item_mismatch.mismatched_mods == 0u);

    CapturedMod changed_mod = mod;
    changed_mod.value = 3;
    const std::multiset<CapturedMod> different_mods{changed_mod};
    const OracleComparison mod_mismatch =
        compare_records(live_items, port_items, live_mods, different_mods);
    CHECK(mod_mismatch.mismatched_items == 0u);
    CHECK(mod_mismatch.mismatched_mods == 2u);
    CHECK(mod_mismatch.details.size() == 2u);
}
