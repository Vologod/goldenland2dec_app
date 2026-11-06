#include <doctest/doctest.h>
#include "gl/assets/cs.hpp"
#include "gl/script/native_registry.hpp"
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <vector>

TEST_CASE("native registry covers every Server.dll id") {
    const auto entries = gl::script::NativeRegistry::entries();
    REQUIRE(entries.size() == 65);
    CHECK(gl::script::NativeRegistry::size() == entries.size());
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
    for (std::uint32_t id : {0x4000000u, 0x4000001u, 0x400000cu})
        CHECK(NativeRegistry::find(id)->support == NativeSupport::supported);
    for (std::uint32_t id :
         {0x5000000u, 0x5000002u, 0x5000004u, 0x5000005u, 0x5000008u})
        CHECK(NativeRegistry::find(id)->support == NativeSupport::supported);
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
        *gl::script::NativeRegistry::find(0x4000002), {}, services);
    CHECK(std::get<std::int32_t>(result) == 0);
}

TEST_CASE("native registry rejects a supported call with missing arguments") {
    gl::script::NativeServices services;
    std::vector<std::string> messages;
    bool played = false;
    services.log = [&](std::string_view text) { messages.emplace_back(text); };
    services.play_sound = [&](std::string_view) { played = true; };

    const gl::script::Value result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000007), {}, services);
    CHECK(std::get<std::int32_t>(result) == 0);
    CHECK_FALSE(played);
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("missing") != std::string::npos);
}

TEST_CASE("native registry rejects a non-numeric Exit argument") {
    gl::script::NativeServices services;
    std::vector<std::string> messages;
    std::int32_t exit_code = -1;
    services.log = [&](std::string_view text) { messages.emplace_back(text); };
    services.exit = [&](std::int32_t code) { exit_code = code; };

    const std::string text = "stop";
    const gl::script::Value argument = text;
    const gl::script::Value result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x1000000), {&argument, 1}, services);
    CHECK(std::get<std::int32_t>(result) == 0);
    CHECK(exit_code == -1);
    REQUIRE(messages.size() == 1);
}

TEST_CASE("native registry clamps extreme Exit arguments safely") {
    gl::script::NativeServices services;
    std::int32_t exit_code = 0;
    services.exit = [&](std::int32_t code) { exit_code = code; };
    const auto call_exit = [&](gl::script::Value value) {
        gl::script::NativeRegistry::call(
            *gl::script::NativeRegistry::find(0x1000000), {&value, 1}, services);
    };

    gl::script::Value infinite = std::numeric_limits<double>::infinity();
    call_exit(infinite);
    CHECK(exit_code == 0);

    gl::script::Value huge = 1e300;
    call_exit(huge);
    CHECK(exit_code == std::numeric_limits<std::int32_t>::max());
}
