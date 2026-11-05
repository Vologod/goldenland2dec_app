#include "gl/script/native_registry.hpp"
#include "gl/assets/cs.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace gl::script {
namespace {
std::int32_t as_int(const Value& value) {
    if (const auto* item = std::get_if<std::int32_t>(&value)) return *item;
    if (const auto* item = std::get_if<std::uint32_t>(&value))
        return static_cast<std::int32_t>(*item);
    if (const auto* item = std::get_if<double>(&value))
        return static_cast<std::int32_t>(*item);
    return 0;
}

std::string_view as_string(const Value& value) {
    if (const auto* item = std::get_if<std::string>(&value)) return *item;
    return {};
}

std::string_view first_string(std::span<const Value> args) {
    return args.empty() ? std::string_view{} : as_string(args.front());
}

Value native_exit(std::span<const Value> args, NativeServices& services) {
    const std::int32_t code = args.empty() ? 0 : as_int(args.front());
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
                                NativeHandler handler = nullptr) {
        entries.push_back(NativeInfo{id, support, reason, handler});
    };

    add(0x1000000, NativeSupport::supported, {}, &native_exit);
    add(0x1000001, NativeSupport::supported, {}, &native_signal);
    add(0x1000002, NativeSupport::supported, {}, &native_console);
    add(0x1000003, NativeSupport::supported, {}, &native_cmd);
    add(0x1000004, NativeSupport::vm_internal, "handled by ScriptVm");
    add(0x1000005, NativeSupport::vm_internal, "handled by ScriptVm");
    add(0x1000006, NativeSupport::vm_internal, "handled by ScriptVm");
    add(0x1000007, NativeSupport::supported, {}, &native_play_sound);

    for (std::uint32_t id = 0x2000000; id <= 0x2000002; ++id)
        add(id, NativeSupport::unsupported, "requires world/effect runtime");
    for (std::uint32_t id = 0x3000000; id <= 0x3000008; ++id)
        add(id, NativeSupport::unsupported, "requires world/level runtime");
    for (std::uint32_t id = 0x4000000; id <= 0x400000c; ++id)
        add(id, NativeSupport::unsupported, "requires person runtime state");
    for (std::uint32_t id = 0x5000000; id <= 0x5000008; ++id)
        add(id, NativeSupport::unsupported, "requires item runtime state");
    for (std::uint32_t id = 0x6000000; id <= 0x6000003; ++id)
        add(id, NativeSupport::unsupported, "requires world clock");
    for (std::uint32_t id = 0x7000000; id <= 0x7000010; ++id)
        add(id, NativeSupport::unsupported, "requires quest/event runtime");
    add(0x7000013, NativeSupport::unsupported, "requires person runtime state");
    add(0x7000014, NativeSupport::unsupported, "requires world/level runtime");
    return entries;
}
} // namespace

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

Value NativeRegistry::call(const NativeInfo& info, std::span<const Value> args,
                           NativeServices& services) {
    if (info.support != NativeSupport::supported || info.handler == nullptr)
        return std::int32_t{0};
    return info.handler(args, services);
}
} // namespace gl::script
