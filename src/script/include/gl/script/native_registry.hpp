#pragma once
#include "gl/script/person_access.hpp"
#include "gl/script/value.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace gl::script {
enum class NativeSupport { vm_internal, supported, unsupported };

struct InventoryAccess {
    std::function<std::int32_t(std::string_view person, std::string_view tech_name)> item_count;
    std::function<std::int32_t()> money;
    std::function<void(std::string_view person, std::string_view tech_name, std::int32_t count)>
        add_item;
    std::function<void(std::string_view person, std::string_view tech_name, std::int32_t count)>
        remove_item;
    std::function<bool(std::string_view from, std::string_view to, std::string_view tech_name,
                       std::int32_t count)>
        transfer_item;
    std::function<bool(std::string_view from, std::string_view to)> transfer_all;
};

struct NativeServices {
    std::function<void(std::string_view)> console;
    std::function<void(std::string_view)> command;
    std::function<void(std::string_view)> signal;
    std::function<void(std::int32_t)> exit;
    std::function<void(std::string_view)> play_sound;
    std::function<void(std::string_view)> log;
    PersonAccess person;
    InventoryAccess inventory;
};

using NativeHandler = Value (*)(std::span<const Value>, NativeServices&);

struct NativeInfo {
    std::uint32_t id = 0;
    NativeSupport support = NativeSupport::unsupported;
    std::string_view reason;
    NativeHandler handler = nullptr;
    std::uint8_t arity = 0;
    const char* name() const;
};

class NativeRegistry {
  public:
    static std::span<const NativeInfo> entries();
    static const NativeInfo* find(std::uint32_t id);
    static std::size_t size();
    static Value call(const NativeInfo& info, std::span<const Value> args,
                      NativeServices& services);
};
} // namespace gl::script
