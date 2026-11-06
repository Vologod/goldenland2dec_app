#include <doctest/doctest.h>
#include "gl/game/game_script_host.hpp"
#include <string>
#include <string_view>
#include <vector>

TEST_CASE("game script host logs an unsupported native once") {
    std::vector<std::string> messages;
    gl::script::NativeServices services;
    services.log = [&](std::string_view text) { messages.emplace_back(text); };
    gl::game::GameScriptHost host(services);
    const auto args = std::span<const gl::script::Value>{};
    CHECK(std::get<std::int32_t>(host.call(0x4000002, args)) == 0);
    CHECK(std::get<std::int32_t>(host.call(0x4000002, args)) == 0);
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("RS_AddPerson_1") != std::string::npos);
    CHECK(messages.front().find("unimplemented") != std::string::npos);
}

TEST_CASE("game script host dispatches supported natives") {
    std::string console;
    gl::script::NativeServices services;
    services.console = [&](std::string_view text) { console = std::string(text); };
    gl::game::GameScriptHost host(services);
    const gl::script::Value argument = std::string("hello");
    host.call(0x1000002, {&argument, 1});
    CHECK(console == "hello");
}

TEST_CASE("game script host ignores unknown ids") {
    gl::game::GameScriptHost host;
    const auto args = std::span<const gl::script::Value>{};
    CHECK(std::get<std::int32_t>(host.call(0xDEADBEEF, args)) == 0);
}

TEST_CASE("game script host ignores vm-internal natives") {
    gl::game::GameScriptHost host;
    const auto args = std::span<const gl::script::Value>{};
    CHECK(std::get<std::int32_t>(host.call(0x1000004, args)) == 0);
}

TEST_CASE("game script host dispatches Signal") {
    std::string signal_text;
    gl::script::NativeServices services;
    services.signal = [&](std::string_view text) { signal_text = std::string(text); };
    gl::game::GameScriptHost host(services);
    const gl::script::Value argument = std::string("alarm");
    host.call(0x1000001, {&argument, 1});
    CHECK(signal_text == "alarm");
}

TEST_CASE("game script host requests stop on a nonzero Exit") {
    gl::game::GameScriptHost host;
    const gl::script::Value code = std::int32_t{1};
    host.call(0x1000000, {&code, 1});
    CHECK(host.stop_requested());
}

TEST_CASE("game script host keeps running on a zero Exit") {
    gl::game::GameScriptHost host;
    const gl::script::Value code = std::int32_t{0};
    host.call(0x1000000, {&code, 1});
    CHECK_FALSE(host.stop_requested());
}
