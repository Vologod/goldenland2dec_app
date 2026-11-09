#include "gl/script/native_registry.hpp"
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::NativeSupport;
using gl::script::Value;

TEST_CASE("trade panel native registry arity and support match Server.dll") {
    const auto* info = NativeRegistry::find(0x700000d);
    REQUIRE(info != nullptr);
    CHECK(info->support == NativeSupport::supported);
    CHECK(info->arity == 0);
    CHECK(info->handler != nullptr);
}

TEST_CASE("trade panel native invokes the callback and returns zero") {
    NativeServices services;
    int calls = 0;
    services.trade_panel = [&]() { ++calls; };

    const Value result = NativeRegistry::call(*NativeRegistry::find(0x700000d), {}, services);
    CHECK(calls == 1);
    REQUIRE(std::holds_alternative<double>(result));
    CHECK(std::get<double>(result) == 0.0);
}

TEST_CASE("trade panel native without a callback logs and returns zero") {
    NativeServices services;
    std::vector<std::string> messages;
    services.log = [&](std::string_view text) { messages.emplace_back(text); };

    const Value result = NativeRegistry::call(*NativeRegistry::find(0x700000d), {}, services);
    REQUIRE(std::holds_alternative<double>(result));
    CHECK(std::get<double>(result) == 0.0);
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("RS_PassToTradePanel") != std::string::npos);
}

TEST_CASE("trade panel native ignores extra arguments") {
    NativeServices services;
    int calls = 0;
    services.trade_panel = [&]() { ++calls; };

    const Value argument = std::int32_t{1};
    const Value result =
        NativeRegistry::call(*NativeRegistry::find(0x700000d), {&argument, 1}, services);
    CHECK(calls == 1);
    REQUIRE(std::holds_alternative<double>(result));
    CHECK(std::get<double>(result) == 0.0);
}

TEST_CASE("native registry reports 16 supported natives out of 65 after trade panel") {
    std::size_t supported = 0;
    for (const auto& entry : NativeRegistry::entries())
        if (entry.support == NativeSupport::supported)
            ++supported;
    CHECK(NativeRegistry::size() == 65);
    CHECK(supported == 16);
}
