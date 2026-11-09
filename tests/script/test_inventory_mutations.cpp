#include <doctest/doctest.h>
#include "gl/script/native_registry.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::NativeSupport;

namespace {
std::vector<gl::script::Value> mutation_args(std::string person, std::string tech,
                                             std::int32_t count) {
    return {gl::script::Value{person}, gl::script::Value{tech},
            gl::script::Value{count}};
}
} // namespace

TEST_CASE("AddItem forwards person, tech and count and returns 0.0") {
    NativeServices services;
    std::string seen_person;
    std::string seen_tech;
    std::int32_t seen_count = 0;
    int calls = 0;
    services.inventory.add_item = [&](std::string_view person, std::string_view tech,
                                      std::int32_t count) {
        seen_person = std::string(person);
        seen_tech = std::string(tech);
        seen_count = count;
        ++calls;
    };
    const auto args = mutation_args("Hero", "MON_1_0_1", 100);
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000004), args, services);
    CHECK(calls == 1);
    CHECK(seen_person == "Hero");
    CHECK(seen_tech == "MON_1_0_1");
    CHECK(seen_count == 100);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("RemoveItem forwards person, tech and count and returns 0.0") {
    NativeServices services;
    std::string seen_person;
    std::string seen_tech;
    std::int32_t seen_count = 0;
    int calls = 0;
    services.inventory.remove_item = [&](std::string_view person, std::string_view tech,
                                         std::int32_t count) {
        seen_person = std::string(person);
        seen_tech = std::string(tech);
        seen_count = count;
        ++calls;
    };
    const auto args = mutation_args("Hero", "TECH(953)", 10);
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000005), args, services);
    CHECK(calls == 1);
    CHECK(seen_person == "Hero");
    CHECK(seen_tech == "TECH(953)");
    CHECK(seen_count == 10);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("mutation natives convert a numeric count with as_int") {
    NativeServices services;
    std::int32_t seen_count = 0;
    services.inventory.add_item = [&](std::string_view, std::string_view,
                                      std::int32_t count) { seen_count = count; };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value tech = std::string("MON_1_0_1");
    const gl::script::Value count = std::int32_t{-3};
    const gl::script::Value args[] = {person, tech, count};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000004), args, services);
    CHECK(seen_count == -3);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("mutation natives return 0.0 without callbacks") {
    NativeServices services;
    const auto args = mutation_args("Hero", "MON_1_0_1", 1);
    for (std::uint32_t id : {0x5000004u, 0x5000005u}) {
        const auto result = NativeRegistry::call(*NativeRegistry::find(id), args, services);
        CHECK(std::get<double>(result) == doctest::Approx(0.0));
    }
}

TEST_CASE("AddItem rejects non-string and non-numeric arguments") {
    NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.inventory.add_item = [&](std::string_view, std::string_view,
                                      std::int32_t) { called = true; };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };

    const auto* info = NativeRegistry::find(0x5000004);
    const gl::script::Value person_number = std::int32_t{7};
    const gl::script::Value tech = std::string("MON_1_0_1");
    const gl::script::Value count = std::int32_t{100};
    const gl::script::Value bad_person_args[] = {person_number, tech, count};
    const auto bad_person =
        NativeRegistry::call(*info, bad_person_args, services);

    const gl::script::Value person = std::string("Hero");
    const gl::script::Value tech_number = std::int32_t{812};
    const gl::script::Value bad_tech_args[] = {person, tech_number, count};
    const auto bad_tech = NativeRegistry::call(*info, bad_tech_args, services);

    const gl::script::Value count_string = std::string("100");
    const gl::script::Value bad_count_args[] = {person, tech, count_string};
    const auto bad_count = NativeRegistry::call(*info, bad_count_args, services);

    CHECK_FALSE(called);
    CHECK(std::get<double>(bad_person) == doctest::Approx(0.0));
    CHECK(std::get<double>(bad_tech) == doctest::Approx(0.0));
    CHECK(std::get<double>(bad_count) == doctest::Approx(0.0));
    REQUIRE(messages.size() == 3);
    for (const auto& message : messages)
        CHECK(message.find("invalid") != std::string::npos);
}

TEST_CASE("RemoveItem rejects a non-string tech argument") {
    NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.inventory.remove_item = [&](std::string_view, std::string_view,
                                         std::int32_t) { called = true; };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };

    const gl::script::Value person = std::string("Hero");
    const gl::script::Value tech_number = std::int32_t{812};
    const gl::script::Value count = std::int32_t{100};
    const gl::script::Value args[] = {person, tech_number, count};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000005), args, services);
    CHECK_FALSE(called);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("invalid") != std::string::npos);
}

TEST_CASE("mutation natives warn and do not call back on missing arguments") {
    NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.inventory.add_item = [&](std::string_view, std::string_view,
                                      std::int32_t) { called = true; };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };

    const gl::script::Value person = std::string("Hero");
    const gl::script::Value tech = std::string("MON_1_0_1");
    const gl::script::Value args[] = {person, tech};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000004), args, services);
    CHECK_FALSE(called);
    CHECK(std::get<std::int32_t>(result) == 0);
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("missing") != std::string::npos);
}

TEST_CASE("mutation native registry arities and support match Server.dll") {
    const auto* add = NativeRegistry::find(0x5000004);
    const auto* remove = NativeRegistry::find(0x5000005);
    REQUIRE(add != nullptr);
    REQUIRE(remove != nullptr);
    CHECK(add->support == NativeSupport::supported);
    CHECK(remove->support == NativeSupport::supported);
    CHECK(add->arity == 3);
    CHECK(remove->arity == 3);
    CHECK(add->handler != nullptr);
    CHECK(remove->handler != nullptr);
    for (std::uint32_t id : {0x5000006u, 0x5000007u}) {
        const auto* info = NativeRegistry::find(id);
        REQUIRE(info != nullptr);
        CHECK(info->support == NativeSupport::unsupported);
        CHECK(info->handler == nullptr);
    }
}

TEST_CASE("native registry reports 16 supported natives out of 65") {
    std::size_t supported = 0;
    for (const auto& entry : NativeRegistry::entries())
        if (entry.support == NativeSupport::supported) ++supported;
    CHECK(NativeRegistry::size() == 65);
    CHECK(supported == 16);
}
