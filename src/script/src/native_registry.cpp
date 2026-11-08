#include "gl/script/native_registry.hpp"
#include "gl/assets/cs.hpp"
#include "gl/assets/log.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace gl::script {
namespace {
std::int32_t as_int(const Value& value) {
    if (const auto* item = std::get_if<std::int32_t>(&value)) return *item;
    if (const auto* item = std::get_if<std::uint32_t>(&value))
        return static_cast<std::int32_t>(*item);
    if (const auto* item = std::get_if<double>(&value)) {
        if (!std::isfinite(*item)) return 0;
        if (*item >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
            return std::numeric_limits<std::int32_t>::max();
        if (*item <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
            return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(*item);
    }
    return 0;
}

std::string_view as_string(const Value& value) {
    if (const auto* item = std::get_if<std::string>(&value)) return *item;
    return {};
}

bool is_string(const Value& value) {
    return std::holds_alternative<std::string>(value);
}

bool is_numeric(const Value& value) {
    return std::holds_alternative<std::int32_t>(value) ||
           std::holds_alternative<std::uint32_t>(value) ||
           std::holds_alternative<double>(value);
}

void warn_native(std::string_view text, NativeServices& services) {
    if (services.log)
        services.log(text);
    else
        gl::log(gl::LogLevel::warning, text);
}

Value native_get_person_parameter(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 2 || !is_string(args[0]) || !is_string(args[1])) {
        warn_native("script native RS_GetPersonParameterI: invalid arguments", services);
        return double{0.0};
    }
    const auto parameter = person_parameter_from_name(as_string(args[1]));
    if (!services.person.get_parameter) return double{0.0};
    return services.person.get_parameter(as_string(args[0]), parameter);
}

Value native_set_person_parameter(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 3 || !is_string(args[0]) || !is_string(args[1]) ||
        !is_numeric(args[2])) {
        warn_native("script native RS_SetPersonParameterI: invalid arguments", services);
        return double{0.0};
    }
    const auto parameter = person_parameter_from_name(as_string(args[1]));
    if (services.person.set_parameter)
        services.person.set_parameter(as_string(args[0]), parameter,
                                      static_cast<double>(as_int(args[2])));
    return double{0.0};
}

Value native_get_person_skill(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 2 || !is_string(args[0]) || !is_string(args[1])) {
        warn_native("script native RS_GetPersonSkillI: invalid arguments", services);
        return double{0.0};
    }
    if (!services.person.get_skill) return double{0.0};
    return services.person.get_skill(as_string(args[0]), as_string(args[1]));
}

Value native_test_person_has_item(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 2 || !is_string(args[0]) || !is_string(args[1])) {
        warn_native("script native RS_TestPersonHasItem: invalid arguments", services);
        return double{0.0};
    }
    if (!services.inventory.item_count) return double{0.0};
    const auto count = services.inventory.item_count(as_string(args[0]), as_string(args[1]));
    return count > 0 ? double{1.0} : double{0.0};
}

Value native_get_item_count(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 2 || !is_string(args[0]) || !is_string(args[1])) {
        warn_native("script native RS_GetItemCountI: invalid arguments", services);
        return double{0.0};
    }
    if (!services.inventory.item_count) return double{0.0};
    return static_cast<double>(
        services.inventory.item_count(as_string(args[0]), as_string(args[1])));
}

Value native_get_money(std::span<const Value>, NativeServices& services) {
    if (!services.inventory.money) return double{0.0};
    return static_cast<double>(services.inventory.money());
}

Value native_add_item(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 3 || !is_string(args[0]) || !is_string(args[1]) ||
        !is_numeric(args[2])) {
        warn_native("script native RS_PersonAddItem: invalid arguments", services);
        return double{0.0};
    }
    if (services.inventory.add_item)
        services.inventory.add_item(as_string(args[0]), as_string(args[1]),
                                    as_int(args[2]));
    return double{0.0};
}

Value native_remove_item(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 3 || !is_string(args[0]) || !is_string(args[1]) ||
        !is_numeric(args[2])) {
        warn_native("script native RS_PersonRemoveItem: invalid arguments", services);
        return double{0.0};
    }
    if (services.inventory.remove_item)
        services.inventory.remove_item(as_string(args[0]), as_string(args[1]),
                                       as_int(args[2]));
    return double{0.0};
}

Value native_transfer_item(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 4 || !is_string(args[0]) || !is_string(args[1]) ||
        !is_string(args[2]) || !is_numeric(args[3])) {
        warn_native("script native RS_PersonTransferItemI: invalid arguments", services);
        return double{0.0};
    }
    if (!services.inventory.transfer_item) return double{0.0};
    const bool transferred =
        services.inventory.transfer_item(as_string(args[0]), as_string(args[1]),
                                         as_string(args[2]), as_int(args[3]));
    return transferred ? double{1.0} : double{0.0};
}

Value native_transfer_all(std::span<const Value> args, NativeServices& services) {
    if (args.size() < 2 || !is_string(args[0]) || !is_string(args[1])) {
        warn_native("script native RS_PersonTransferAllItemsI: invalid arguments",
                    services);
        return double{0.0};
    }
    if (!services.inventory.transfer_all) return double{0.0};
    const bool transferred =
        services.inventory.transfer_all(as_string(args[0]), as_string(args[1]));
    return transferred ? double{1.0} : double{0.0};
}

std::string_view first_string(std::span<const Value> args) {
    return args.empty() ? std::string_view{} : as_string(args.front());
}

Value native_exit(std::span<const Value> args, NativeServices& services) {
    if (args.empty() || std::holds_alternative<std::string>(args.front())) {
        if (services.log) services.log("script native Exit: expected a numeric argument");
        return std::int32_t{0};
    }
    const std::int32_t code = as_int(args.front());
    if (services.exit) services.exit(code);
    return std::int32_t{0};
}

Value native_signal(std::span<const Value> args, NativeServices& services) {
    if (services.signal) services.signal(first_string(args));
    return std::int32_t{0};
}

Value native_console(std::span<const Value> args, NativeServices& services) {
    if (services.console) services.console(first_string(args));
    return std::int32_t{0};
}

Value native_cmd(std::span<const Value> args, NativeServices& services) {
    if (services.command) services.command(first_string(args));
    return std::int32_t{0};
}

Value native_play_sound(std::span<const Value> args, NativeServices& services) {
    if (services.play_sound) services.play_sound(first_string(args));
    return std::int32_t{0};
}

std::vector<NativeInfo> make_entries() {
    std::vector<NativeInfo> entries;
    entries.reserve(65);
    const auto add = [&entries](std::uint32_t id, NativeSupport support,
                                std::string_view reason = {},
                                NativeHandler handler = nullptr, std::uint8_t arity = 0) {
        entries.push_back(NativeInfo{id, support, reason, handler, arity});
    };

    add(0x1000000, NativeSupport::supported, {}, &native_exit, 1);
    add(0x1000001, NativeSupport::supported, {}, &native_signal, 1);
    add(0x1000002, NativeSupport::supported, {}, &native_console, 1);
    add(0x1000003, NativeSupport::supported, {}, &native_cmd, 1);
    add(0x1000004, NativeSupport::vm_internal, "handled by ScriptVm", nullptr, 1);
    add(0x1000005, NativeSupport::vm_internal, "handled by ScriptVm", nullptr, 1);
    add(0x1000006, NativeSupport::vm_internal, "handled by ScriptVm", nullptr, 1);
    add(0x1000007, NativeSupport::supported, {}, &native_play_sound, 1);

    for (std::uint32_t id = 0x2000000; id <= 0x2000002; ++id)
        add(id, NativeSupport::unsupported, "requires world/effect runtime");
    for (std::uint32_t id = 0x3000000; id <= 0x3000008; ++id)
        add(id, NativeSupport::unsupported, "requires world/level runtime");
    add(0x4000000, NativeSupport::supported, {}, &native_get_person_parameter, 2);
    add(0x4000001, NativeSupport::supported, {}, &native_set_person_parameter, 3);
    add(0x400000c, NativeSupport::supported, {}, &native_get_person_skill, 2);
    for (std::uint32_t id = 0x4000000; id <= 0x400000c; ++id)
        if (id != 0x4000000 && id != 0x4000001 && id != 0x400000c)
            add(id, NativeSupport::unsupported, "requires person runtime state");
    add(0x5000000, NativeSupport::supported, {}, &native_test_person_has_item, 2);
    add(0x5000001, NativeSupport::supported, {}, &native_transfer_item, 4);
    add(0x5000002, NativeSupport::supported, {}, &native_get_item_count, 2);
    add(0x5000003, NativeSupport::supported, {}, &native_transfer_all, 2);
    add(0x5000004, NativeSupport::supported, {}, &native_add_item, 3);
    add(0x5000005, NativeSupport::supported, {}, &native_remove_item, 3);
    add(0x5000008, NativeSupport::supported, {}, &native_get_money, 0);
    for (std::uint32_t id = 0x5000000; id <= 0x5000008; ++id)
        if (id != 0x5000000 && id != 0x5000001 && id != 0x5000002 &&
            id != 0x5000003 && id != 0x5000004 && id != 0x5000005 &&
            id != 0x5000008)
            add(id, NativeSupport::unsupported, "requires item runtime state");
    for (std::uint32_t id = 0x6000000; id <= 0x6000003; ++id)
        add(id, NativeSupport::unsupported, "requires world clock");
    for (std::uint32_t id = 0x7000000; id <= 0x7000010; ++id)
        add(id, NativeSupport::unsupported, "requires quest/event runtime");
    add(0x7000013, NativeSupport::unsupported, "requires person runtime state");
    add(0x7000014, NativeSupport::unsupported, "requires world/level runtime");

#ifndef NDEBUG
    for (std::size_t i = 0; i < entries.size(); ++i)
        for (std::size_t j = i + 1; j < entries.size(); ++j)
            assert(entries[i].id != entries[j].id);
#endif
    return entries;
}
} // namespace

PersonParameter person_parameter_from_name(std::string_view name) {
    if (name == "EXPERIENCE") return PersonParameter::experience;
    if (name == "REPUTATION") return PersonParameter::reputation;
    if (name == "INTELLIGENCE") return PersonParameter::intelligence;
    if (name == "WISDOM") return PersonParameter::wisdom;
    if (name == "STRENGTH") return PersonParameter::strength;
    if (name == "CONSTITUTION") return PersonParameter::constitution;
    if (name == "DEXTERITY") return PersonParameter::dexterity;
    if (name == "LUCK") return PersonParameter::luck;
    if (name == "PERCEPTION") return PersonParameter::perception;
    if (name == "MAXIMUM LIFE") return PersonParameter::maximum_life;
    if (name == "LIFE") return PersonParameter::life;
    if (name == "ENERGY") return PersonParameter::energy;
    return PersonParameter::unknown;
}

const char* NativeInfo::name() const { return gl::assets::cs_function_name(id); }

std::span<const NativeInfo> NativeRegistry::entries() {
    static const std::vector<NativeInfo> list = make_entries();
    return list;
}

const NativeInfo* NativeRegistry::find(std::uint32_t id) {
    const auto all = entries();
    const auto it = std::find_if(all.begin(), all.end(),
                                 [id](const NativeInfo& entry) { return entry.id == id; });
    return it == all.end() ? nullptr : &*it;
}

std::size_t NativeRegistry::size() { return entries().size(); }

Value NativeRegistry::call(const NativeInfo& info, std::span<const Value> args,
                           NativeServices& services) {
    if (info.support != NativeSupport::supported || info.handler == nullptr)
        return std::int32_t{0};
    if (args.size() < info.arity) {
        const std::string message =
            std::string("script native ") + info.name() + ": missing arguments";
        if (services.log)
            services.log(message);
        else
            gl::log(gl::LogLevel::warning, message);
        return std::int32_t{0};
    }
    return info.handler(args, services);
}
} // namespace gl::script
