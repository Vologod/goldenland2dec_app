#include <doctest/doctest.h>
#include "gl/script/native_registry.hpp"
#include <string>
#include <string_view>
#include <vector>

using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::NativeSupport;

TEST_CASE("inventory TestPersonHasItem returns 1.0 when count is positive") {
    NativeServices services;
    std::string seen_person;
    std::string seen_tech;
    services.inventory.item_count = [&](std::string_view person, std::string_view tech_name) {
        seen_person = std::string(person);
        seen_tech = std::string(tech_name);
        return 5;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value tech = std::string("MON_1_0_1");
    const gl::script::Value args[] = {person, tech};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000000), args, services);
    CHECK(seen_person == "Hero");
    CHECK(seen_tech == "MON_1_0_1");
    CHECK(std::get<double>(result) == doctest::Approx(1.0));
}

TEST_CASE("inventory TestPersonHasItem returns 0.0 when count is zero") {
    NativeServices services;
    services.inventory.item_count = [](std::string_view, std::string_view) {
        return 0;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value tech = std::string("QST_1_0_159");
    const gl::script::Value args[] = {person, tech};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000000), args, services);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("inventory GetItemCountI returns the count as double") {
    NativeServices services;
    std::string seen_person;
    std::string seen_tech;
    services.inventory.item_count = [&](std::string_view person, std::string_view tech_name) {
        seen_person = std::string(person);
        seen_tech = std::string(tech_name);
        return 43875;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value tech = std::string("MON_1_0_1");
    const gl::script::Value args[] = {person, tech};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000002), args, services);
    CHECK(seen_person == "Hero");
    CHECK(seen_tech == "MON_1_0_1");
    CHECK(std::get<double>(result) == doctest::Approx(43875.0));
}

TEST_CASE("inventory GetMoney returns money as double") {
    NativeServices services;
    bool called = false;
    services.inventory.money = [&]() {
        called = true;
        return 122008;
    };
    const auto result = NativeRegistry::call(*NativeRegistry::find(0x5000008), {}, services);
    CHECK(called);
    CHECK(std::get<double>(result) == doctest::Approx(122008.0));
}

TEST_CASE("inventory natives return 0.0 without callbacks") {
    NativeServices services;
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value tech = std::string("MON_1_0_1");
    const gl::script::Value args[] = {person, tech};
    for (std::uint32_t id : {0x5000000u, 0x5000002u}) {
        const auto result = NativeRegistry::call(*NativeRegistry::find(id), args, services);
        CHECK(std::get<double>(result) == doctest::Approx(0.0));
    }
    const auto money = NativeRegistry::call(*NativeRegistry::find(0x5000008), {}, services);
    CHECK(std::get<double>(money) == doctest::Approx(0.0));
}

TEST_CASE("inventory TestPersonHasItem rejects non-string arguments") {
    NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.inventory.item_count = [&](std::string_view, std::string_view) {
        called = true;
        return 1;
    };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };
    const gl::script::Value person = std::int32_t{7};
    const gl::script::Value tech = std::string("MON_1_0_1");
    const gl::script::Value args[] = {person, tech};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000000), args, services);
    CHECK_FALSE(called);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("invalid") != std::string::npos);
}

TEST_CASE("inventory GetItemCountI rejects non-string arguments") {
    NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.inventory.item_count = [&](std::string_view, std::string_view) {
        called = true;
        return 1;
    };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value tech = std::int32_t{812};
    const gl::script::Value args[] = {person, tech};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000002), args, services);
    CHECK_FALSE(called);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("invalid") != std::string::npos);
}

TEST_CASE("inventory native registry arities and support match Server.dll") {
    const auto* test = NativeRegistry::find(0x5000000);
    const auto* count = NativeRegistry::find(0x5000002);
    const auto* money = NativeRegistry::find(0x5000008);
    REQUIRE(test != nullptr);
    REQUIRE(count != nullptr);
    REQUIRE(money != nullptr);
    CHECK(test->support == NativeSupport::supported);
    CHECK(count->support == NativeSupport::supported);
    CHECK(money->support == NativeSupport::supported);
    CHECK(test->arity == 2);
    CHECK(count->arity == 2);
    CHECK(money->arity == 0);
    CHECK(test->handler != nullptr);
    CHECK(count->handler != nullptr);
    CHECK(money->handler != nullptr);
    for (std::uint32_t id : {0x5000001u, 0x5000003u, 0x5000004u, 0x5000005u,
                             0x5000006u, 0x5000007u}) {
        const auto* info = NativeRegistry::find(id);
        REQUIRE(info != nullptr);
        CHECK(info->support == NativeSupport::unsupported);
        CHECK(info->handler == nullptr);
    }
}
