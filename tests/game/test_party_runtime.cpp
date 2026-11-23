#include "gl/game/party_runtime.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/script/native_registry.hpp"

#include <doctest/doctest.h>
#include <span>
#include <string>
#include <vector>

using gl::game::PartyRuntime;
using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::Value;

namespace {
Value call(std::uint32_t id, std::string name, NativeServices &services) {
    const std::vector<Value> args{name};
    const auto *info = NativeRegistry::find(id);
    REQUIRE(info != nullptr);
    return NativeRegistry::call(*info, args, services);
}
} // namespace

TEST_CASE("party runtime add/remove return slots left") {
    PartyRuntime party;
    CHECK(party.remove("A") == 4);
    CHECK(party.add("A") == 3);
    CHECK(party.add("B") == 2);
    CHECK(party.size() == 2);
    CHECK(party.has("A"));
    CHECK_FALSE(party.has("C"));
    CHECK(party.remove("A") == 3);
    CHECK_FALSE(party.has("A"));
    CHECK(party.remove("missing") == 3);
    CHECK(party.add("C") == 2);
    CHECK(party.add("D") == 1);
    CHECK(party.add("E") == 0);
    CHECK(party.add("F") == -1);
    CHECK(party.add("G") == -1);
    CHECK(party.size() == 4);
    party.clear();
    CHECK(party.add("H") == 3);
    CHECK(party.add("H") == 2);
    CHECK(party.size() == 2);
    party.clear();
    CHECK(party.size() == 0);
}

TEST_CASE("party natives dispatch names and returns") {
    PartyRuntime party;
    NativeServices services;
    services.party = party.access();

    CHECK(call(0x4000007, "A", services) == Value{double{3.0}});
    CHECK(call(0x4000009, "A", services) == Value{double{1.0}});
    CHECK(call(0x4000008, "A", services) == Value{double{4.0}});
    CHECK(call(0x4000009, "A", services) == Value{double{0.0}});
}

TEST_CASE("party natives fail without a runtime") {
    NativeServices services;
    CHECK(call(0x4000007, "A", services) == Value{double{0.0}});
    CHECK(call(0x4000008, "A", services) == Value{double{0.0}});
    CHECK(call(0x4000009, "A", services) == Value{double{0.0}});
}

TEST_CASE("party natives are registered with server arities") {
    for (const std::uint32_t id : {0x4000007u, 0x4000008u, 0x4000009u}) {
        const auto *info = NativeRegistry::find(id);
        REQUIRE(info != nullptr);
        CHECK(info->support == gl::script::NativeSupport::supported);
        CHECK(info->arity == 1);
    }
}

TEST_CASE("party runtime updates entity roles") {
    auto persons = gl::game::PersonRuntime::create({}, std::nullopt);
    PartyRuntime party;
    party.attach(&persons);
    CHECK(party.add("Hero") == 3);
    CHECK(persons.party_role("Hero") == 1);
    party.clear();
    CHECK(persons.party_role("Hero") == 0);
    CHECK(party.add("Hero") == 3);
    CHECK(persons.party_role("Hero") == 1);
    CHECK(party.remove("Hero") == 4);
    CHECK(persons.party_role("Hero") == 2);
    CHECK(party.remove("Hero") == 4);
    CHECK(persons.party_role("Hero") == 2);
    party.clear();
    CHECK(persons.party_role("Hero") == 2);
    CHECK(party.add("Hero") == 3);
    CHECK(persons.party_role("Hero") == 1);
}

TEST_CASE("party role edge cases follow server party flags") {
    auto persons = gl::game::PersonRuntime::create({}, std::nullopt);
    PartyRuntime party;
    party.attach(&persons);

    CHECK(party.remove("Hero") == 4);
    CHECK(persons.party_role("Hero") == 0);

    for (int i = 0; i < 4; ++i) CHECK(party.add("member" + std::to_string(i)) >= 0);
    persons.set_party_role("Hero", 2);
    CHECK(party.add("Hero") == -1);
    CHECK(persons.party_role("Hero") == 2);

    CHECK(party.remove("Hero") == 0);
    CHECK(persons.party_role("Hero") == 2);

    PartyRuntime detached;
    detached.attach(nullptr);
    CHECK(detached.add("X") == 3);

    PartyRuntime single;
    auto members = gl::game::PersonRuntime::create({}, std::nullopt);
    single.attach(&members);
    CHECK(single.add("Hero") == 3);
    CHECK(single.remove("missing") == 3);
    CHECK(members.party_role("Hero") == 1);
}

TEST_CASE("duplicate party members are re-promoted on attach") {
    auto persons = gl::game::PersonRuntime::create({}, std::nullopt);
    PartyRuntime party;
    party.attach(&persons);
    CHECK(party.add("Hero") == 3);
    CHECK(party.add("Hero") == 2);
    CHECK(party.remove("Hero") == 3);
    CHECK(party.has("Hero"));
    CHECK(persons.party_role("Hero") == 2);
    party.attach(&persons);
    CHECK(persons.party_role("Hero") == 1);
}

TEST_CASE("party roles are restored after a person runtime rebuild") {
    auto persons = gl::game::PersonRuntime::create({}, std::nullopt);
    PartyRuntime party;
    party.attach(&persons);
    CHECK(party.add("Hero") == 3);

    auto rebuilt = gl::game::PersonRuntime::create({}, std::nullopt);
    CHECK(rebuilt.party_role("Hero") == 0);
    party.attach(&rebuilt);
    CHECK(rebuilt.party_role("Hero") == 1);
    CHECK(party.size() == 1);
}
