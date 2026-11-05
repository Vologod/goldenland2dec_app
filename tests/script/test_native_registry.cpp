#include <doctest/doctest.h>
#include "gl/assets/cs.hpp"
#include "gl/script/native_registry.hpp"
#include <set>
#include <string>
#include <string_view>

TEST_CASE("native registry covers every Server.dll id") {
    const auto entries = gl::script::NativeRegistry::entries();
    REQUIRE(entries.size() == 65);
    std::set<std::uint32_t> ids;
    for (const auto& entry : entries) {
        CHECK(ids.insert(entry.id).second);
        CHECK(std::string_view(gl::assets::cs_function_name(entry.id)) != "unknown");
        if (entry.support == gl::script::NativeSupport::supported)
            CHECK(entry.handler != nullptr);
        if (entry.support == gl::script::NativeSupport::unsupported)
            CHECK(!entry.reason.empty());
    }
}

TEST_CASE("native registry classifies dialog and host natives") {
    using gl::script::NativeRegistry;
    using gl::script::NativeSupport;
    for (std::uint32_t id : {0x1000004u, 0x1000005u, 0x1000006u})
        CHECK(NativeRegistry::find(id)->support == NativeSupport::vm_internal);
    for (std::uint32_t id : {0x1000000u, 0x1000001u, 0x1000002u, 0x1000003u, 0x1000007u})
        CHECK(NativeRegistry::find(id)->support == NativeSupport::supported);
    CHECK(NativeRegistry::find(0x4000001)->support == NativeSupport::unsupported);
    CHECK(NativeRegistry::find(0xDEADBEEF) == nullptr);
}

TEST_CASE("native registry dispatches supported services") {
    gl::script::NativeServices services;
    std::string console;
    std::string command;
    std::string sound;
    std::int32_t exit_code = -1;
    services.console = [&](std::string_view s) { console = std::string(s); };
    services.command = [&](std::string_view s) { command = std::string(s); };
    services.play_sound = [&](std::string_view s) { sound = std::string(s); };
    services.exit = [&](std::int32_t code) { exit_code = code; };

    const std::string text = "hello";
    const gl::script::Value arg = text;
    gl::script::Value result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000002), {&arg, 1}, services);
    CHECK(console == "hello");
    CHECK(std::get<std::int32_t>(result) == 0);

    gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000003), {&arg, 1}, services);
    CHECK(command == "hello");

    gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000007), {&arg, 1}, services);
    CHECK(sound == "hello");

    const gl::script::Value code = std::int32_t{7};
    gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000000), {&code, 1}, services);
    CHECK(exit_code == 7);
}

TEST_CASE("unsupported native returns zero") {
    gl::script::NativeServices services;
    gl::script::Value result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000000), {}, services);
    CHECK(std::get<std::int32_t>(result) == 0);
}
