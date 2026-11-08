#include <doctest/doctest.h>
#include "gl/script/native_registry.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::NativeSupport;

namespace {
std::vector<gl::script::Value> item_args(std::string from, std::string to,
                                         std::string tech, std::int32_t count) {
    return {gl::script::Value{std::move(from)}, gl::script::Value{std::move(to)},
            gl::script::Value{std::move(tech)}, gl::script::Value{count}};
}

std::vector<gl::script::Value> all_args(std::string from, std::string to) {
    return {gl::script::Value{std::move(from)}, gl::script::Value{std::move(to)}};
}
} // namespace

TEST_CASE("TransferItem forwards source, dest, tech and count and returns 1.0") {
    NativeServices services;
    std::string seen_from;
    std::string seen_to;
    std::string seen_tech;
    std::int32_t seen_count = 0;
    int calls = 0;
    services.inventory.transfer_item = [&](std::string_view from, std::string_view to,
                                           std::string_view tech, std::int32_t count) {
        seen_from = std::string(from);
        seen_to = std::string(to);
        seen_tech = std::string(tech);
        seen_count = count;
        ++calls;
        return true;
    };
    const auto args = item_args("Hero", "L1_1.P387_Poison_Plotnik", "QST_1_0_3", 1);
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000001), args, services);
    CHECK(calls == 1);
    CHECK(seen_from == "Hero");
    CHECK(seen_to == "L1_1.P387_Poison_Plotnik");
    CHECK(seen_tech == "QST_1_0_3");
    CHECK(seen_count == 1);
    CHECK(std::get<double>(result) == doctest::Approx(1.0));
}

TEST_CASE("TransferItem returns 0.0 when the callback reports failure") {
    NativeServices services;
    services.inventory.transfer_item = [](std::string_view, std::string_view,
                                          std::string_view, std::int32_t) {
        return false;
    };
    const auto args = item_args("Hero", "L1_1.P387_Poison_Plotnik", "QST_1_0_3", 5);
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000001), args, services);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("TransferItem converts a numeric count with as_int") {
    NativeServices services;
    std::int32_t seen_count = 0;
    services.inventory.transfer_item = [&](std::string_view, std::string_view,
                                           std::string_view, std::int32_t count) {
        seen_count = count;
        return true;
    };
    const gl::script::Value from = std::string("Hero");
    const gl::script::Value to = std::string("L1_1.P387_Poison_Plotnik");
    const gl::script::Value tech = std::string("QST_1_0_3");
    const gl::script::Value count = 2.75;
    const gl::script::Value args[] = {from, to, tech, count};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000001), args, services);
    CHECK(seen_count == 2);
    CHECK(std::get<double>(result) == doctest::Approx(1.0));
}

TEST_CASE("TransferItem returns 0.0 without a callback") {
    NativeServices services;
    const auto args = item_args("Hero", "L1_1.P387_Poison_Plotnik", "QST_1_0_3", 1);
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000001), args, services);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("TransferAll forwards source and dest and returns 1.0") {
    NativeServices services;
    std::string seen_from;
    std::string seen_to;
    int calls = 0;
    services.inventory.transfer_all = [&](std::string_view from, std::string_view to) {
        seen_from = std::string(from);
        seen_to = std::string(to);
        ++calls;
        return true;
    };
    const auto args = all_args("L1_1.P387_Poison_Plotnik", "Hero");
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000003), args, services);
    CHECK(calls == 1);
    CHECK(seen_from == "L1_1.P387_Poison_Plotnik");
    CHECK(seen_to == "Hero");
    CHECK(std::get<double>(result) == doctest::Approx(1.0));
}

TEST_CASE("TransferAll returns 0.0 when the callback reports failure") {
    NativeServices services;
    services.inventory.transfer_all = [](std::string_view, std::string_view) {
        return false;
    };
    const auto args = all_args("L1_1.P387_Poison_Plotnik", "Hero");
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000003), args, services);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("TransferAll returns 0.0 without a callback") {
    NativeServices services;
    const auto args = all_args("L1_1.P387_Poison_Plotnik", "Hero");
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000003), args, services);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("TransferItem rejects non-string arguments") {
    NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.inventory.transfer_item = [&](std::string_view, std::string_view,
                                           std::string_view, std::int32_t) {
        called = true;
        return true;
    };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };

    const auto* info = NativeRegistry::find(0x5000001);
    const gl::script::Value number = std::int32_t{7};
    const gl::script::Value from = std::string("Hero");
    const gl::script::Value to = std::string("L1_1.P387_Poison_Plotnik");
    const gl::script::Value tech = std::string("QST_1_0_3");
    const gl::script::Value count = std::int32_t{1};

    const gl::script::Value bad_from_args[] = {number, to, tech, count};
    const auto bad_from = NativeRegistry::call(*info, bad_from_args, services);
    const gl::script::Value bad_to_args[] = {from, number, tech, count};
    const auto bad_to = NativeRegistry::call(*info, bad_to_args, services);
    const gl::script::Value bad_tech_args[] = {from, to, number, count};
    const auto bad_tech = NativeRegistry::call(*info, bad_tech_args, services);

    CHECK_FALSE(called);
    CHECK(std::get<double>(bad_from) == doctest::Approx(0.0));
    CHECK(std::get<double>(bad_to) == doctest::Approx(0.0));
    CHECK(std::get<double>(bad_tech) == doctest::Approx(0.0));
    REQUIRE(messages.size() == 3);
    for (const auto& message : messages)
        CHECK(message.find("invalid") != std::string::npos);
}

TEST_CASE("TransferItem rejects a non-numeric count") {
    NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.inventory.transfer_item = [&](std::string_view, std::string_view,
                                           std::string_view, std::int32_t) {
        called = true;
        return true;
    };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };

    const gl::script::Value from = std::string("Hero");
    const gl::script::Value to = std::string("L1_1.P387_Poison_Plotnik");
    const gl::script::Value tech = std::string("QST_1_0_3");
    const gl::script::Value count = std::string("1");
    const gl::script::Value args[] = {from, to, tech, count};
    const auto result =
        NativeRegistry::call(*NativeRegistry::find(0x5000001), args, services);
    CHECK_FALSE(called);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("invalid") != std::string::npos);
}

TEST_CASE("TransferAll rejects non-string arguments") {
    NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.inventory.transfer_all = [&](std::string_view, std::string_view) {
        called = true;
        return true;
    };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };

    const auto* info = NativeRegistry::find(0x5000003);
    const gl::script::Value number = std::int32_t{7};
    const gl::script::Value from = std::string("L1_1.P387_Poison_Plotnik");
    const gl::script::Value to = std::string("Hero");

    const gl::script::Value bad_from_args[] = {number, to};
    const auto bad_from = NativeRegistry::call(*info, bad_from_args, services);
    const gl::script::Value bad_to_args[] = {from, number};
    const auto bad_to = NativeRegistry::call(*info, bad_to_args, services);

    CHECK_FALSE(called);
    CHECK(std::get<double>(bad_from) == doctest::Approx(0.0));
    CHECK(std::get<double>(bad_to) == doctest::Approx(0.0));
    REQUIRE(messages.size() == 2);
    for (const auto& message : messages)
        CHECK(message.find("invalid") != std::string::npos);
}

TEST_CASE("transfer natives warn and return zero on missing arguments") {
    NativeServices services;
    std::vector<std::string> messages;
    services.log = [&](std::string_view text) { messages.emplace_back(text); };

    const gl::script::Value from = std::string("Hero");
    const gl::script::Value to = std::string("L1_1.P387_Poison_Plotnik");
    const gl::script::Value item_missing[] = {from, to};
    const gl::script::Value all_missing[] = {from};

    const auto item = NativeRegistry::call(
        *NativeRegistry::find(0x5000001), item_missing, services);
    const auto all = NativeRegistry::call(
        *NativeRegistry::find(0x5000003), all_missing, services);
    CHECK(std::get<std::int32_t>(item) == 0);
    CHECK(std::get<std::int32_t>(all) == 0);
    REQUIRE(messages.size() == 2);
    for (const auto& message : messages)
        CHECK(message.find("missing") != std::string::npos);
}

TEST_CASE("transfer native registry arities and support match Server.dll") {
    const auto* item = NativeRegistry::find(0x5000001);
    const auto* all = NativeRegistry::find(0x5000003);
    REQUIRE(item != nullptr);
    REQUIRE(all != nullptr);
    CHECK(item->support == NativeSupport::supported);
    CHECK(all->support == NativeSupport::supported);
    CHECK(item->arity == 4);
    CHECK(all->arity == 2);
    CHECK(item->handler != nullptr);
    CHECK(all->handler != nullptr);
    for (std::uint32_t id : {0x5000006u, 0x5000007u}) {
        const auto* info = NativeRegistry::find(id);
        REQUIRE(info != nullptr);
        CHECK(info->support == NativeSupport::unsupported);
        CHECK(info->handler == nullptr);
    }
}

TEST_CASE("native registry reports 15 supported natives out of 65 after transfers") {
    std::size_t supported = 0;
    for (const auto& entry : NativeRegistry::entries())
        if (entry.support == NativeSupport::supported) ++supported;
    CHECK(NativeRegistry::size() == 65);
    CHECK(supported == 15);
}
