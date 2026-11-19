#include "gl/game/live_save.hpp"

#include "gl/assets/hero_save_body.hpp"
#include "gl/assets/save_item_convert.hpp"
#include "gl/game/hero_save_write.hpp"
#include "gl/game/save_person.hpp"
#include "gl/game/tail_write.hpp"

#include <cstddef>
#include <map>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace gl::game {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

Result<std::vector<std::uint8_t>>
hero_body(const gl::assets::SavePayload &payload, const HeroLiveState &hero) {
    if (payload.blocks.size() <= 6)
        return format_error("live save: BT_HERO block is missing");
    const auto &block = payload.blocks[6];
    if (block.tag != "BT_HERO ")
        return format_error("live save: BT_HERO block order mismatch");
    const auto body = gl::assets::parse_hero_save_body(
        std::span<const std::uint8_t>(payload.decompressed)
            .subspan(block.offset, block.size));
    if (!body)
        return body.error();
    const auto rebuilt = make_hero_save_body(body.value(), hero);
    return gl::assets::encode_hero_save_body(rebuilt);
}
} // namespace

Result<gl::assets::SavePayload> apply_live_save(gl::assets::SavePayload payload,
                                                const LiveSaveInput &input) {
    try {
        if (input.hero != nullptr) {
            auto body = hero_body(payload, *input.hero);
            if (!body)
                return body.error();
            auto replaced = gl::assets::replace_block_body(payload, 6, body.value());
            if (!replaced)
                return replaced.error();
        }

        if (input.calendar != nullptr) {
            if (!valid_datetime(input.calendar->now()))
                return format_error("live save: calendar datetime is invalid");
            payload.info.time = input.calendar->now().to_time_bytes();
            auto info = gl::assets::update_info_block(payload);
            if (!info)
                return info.error();
        }

        if (input.level != nullptr && input.simulation != nullptr &&
            input.inventory != nullptr && input.restore != nullptr &&
            payload.level_tail_index.has_value()) {
            const gl::assets::SaveTailEntry &loaded =
                payload.tail[*payload.level_tail_index];
            std::vector<TailPersonLive> live;
            live.reserve(input.restore->persons.size());
            std::map<std::string, std::size_t, std::less<>> name_counts;
            for (const PersonInstance &person : input.level->persons)
                ++name_counts[person.name];
            for (const RestoredPerson &restored : input.restore->persons) {
                TailPersonLive person;
                if (restored.level_index >= input.level->persons.size())
                    return format_error("live save: restored level index is out of range");
                const std::string &name = input.level->persons[restored.level_index].name;
                if (input.persons != nullptr && name_counts[name] == 1 &&
                    input.persons->has_parameter(name, gl::script::PersonParameter::life)) {
                    const double value =
                        input.persons->get_parameter(name, gl::script::PersonParameter::life);
                    person.alive = value > 0.0;
                    person.hp = static_cast<std::int32_t>(value);
                }
                if (input.persons != nullptr && name_counts[name] == 1 &&
                    input.persons->has_parameter(name, gl::script::PersonParameter::energy)) {
                    person.energy = static_cast<std::int32_t>(
                        input.persons->get_parameter(name, gl::script::PersonParameter::energy));
                }
                live.push_back(person);
            }
            gl::assets::SaveRginf rginf = loaded.rginf;
            if (input.has_rginf_latch)
                rginf.latch = input.rginf_latch;
            if (input.rginf_datetime)
                rginf.datetime = input.rginf_datetime->to_array();
            const std::vector<std::size_t> no_new_persons;
            auto built = build_selected_tail(loaded, *input.level, *input.simulation,
                                             *input.inventory, *input.restore, live,
                                             no_new_persons, rginf, &payload.tribes);
            if (!built)
                return built.error();
            auto index = gl::assets::upsert_tail_entry(payload, std::move(built.value()));
            if (!index)
                return index.error();
            payload.level_tail_index = index.value();
        }

        if (!input.vars.empty()) {
            auto vars_body = gl::assets::encode_vars_body(input.vars);
            if (!vars_body)
                return vars_body.error();
            auto replaced_vars = gl::assets::replace_block_body(payload, 2, vars_body.value());
            if (!replaced_vars)
                return replaced_vars.error();
        }
        return payload;
    } catch (const std::bad_alloc &) {
        return format_error("live save allocation failed");
    } catch (const std::length_error &) {
        return format_error("live save allocation failed");
    }
}
} // namespace gl::game
