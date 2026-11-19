#include "gl/game/quest_runtime.hpp"
#include "gl/script/native_registry.hpp"

#include <doctest/doctest.h>
#include <span>
#include <string>
#include <vector>

using gl::game::QuestRuntime;
using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::Value;

namespace {
Value call(std::uint32_t id, std::initializer_list<Value> args, NativeServices &services) {
    const std::vector<Value> values(args);
    const auto *info = NativeRegistry::find(id);
    REQUIRE(info != nullptr);
    return NativeRegistry::call(*info, values, services);
}
} // namespace

TEST_CASE("quest runtime tracks quest and stage transitions") {
    QuestRuntime runtime;
    CHECK_FALSE(runtime.quest_enabled("potion"));
    runtime.enable_quest("potion");
    CHECK(runtime.quest_enabled("potion"));
    CHECK_FALSE(runtime.quest_completed("potion"));
    runtime.complete_quest("potion");
    CHECK(runtime.quest_completed("potion"));

    runtime.enable_stage("potion", "potion_4");
    CHECK(runtime.stage_enabled("potion", "potion_4"));
    CHECK_FALSE(runtime.stage_completed("potion", "potion_4"));
    runtime.complete_stage("potion", "potion_4");
    CHECK(runtime.stage_completed("potion", "potion_4"));
    CHECK(runtime.stage_enabled("potion", "potion_4"));
    CHECK_FALSE(runtime.stage_enabled("potion", "potion_5"));
}

TEST_CASE("quest runtime is idempotent and ignores empty ids") {
    QuestRuntime runtime;
    runtime.enable_quest("", true);
    runtime.complete_quest("");
    runtime.enable_stage("", "stage");
    runtime.complete_stage("quest", "");
    CHECK_FALSE(runtime.quest_enabled(""));
    CHECK_FALSE(runtime.stage_enabled("", "stage"));

    runtime.enable_quest("potion", true);
    runtime.enable_quest("potion");
    CHECK(runtime.quest_storyline("potion"));
    runtime.complete_quest("potion");
    runtime.complete_quest("potion");
    CHECK(runtime.quest_completed("potion"));
}

TEST_CASE("quest runtime clear resets all state") {
    QuestRuntime runtime;
    runtime.enable_stage("potion", "potion_4");
    runtime.complete_quest("potion");
    runtime.enable_quest("potion", true);
    runtime.clear();
    CHECK_FALSE(runtime.quest_enabled("potion"));
    CHECK_FALSE(runtime.quest_storyline("potion"));
    CHECK_FALSE(runtime.stage_enabled("potion", "potion_4"));
}

TEST_CASE("quest natives dispatch string names and return zero") {
    QuestRuntime runtime;
    NativeServices services;
    services.quest = runtime.access();

    CHECK(call(0x7000002, {std::string{"potion"}, std::string{"potion_4"}}, services) ==
          Value{double{0.0}});
    CHECK(runtime.quest_enabled("potion"));
    CHECK(runtime.stage_enabled("potion", "potion_4"));
    CHECK(call(0x7000001, {std::string{"potion"}, std::string{"potion_5"}}, services) ==
          Value{double{0.0}});
    CHECK(runtime.stage_enabled("potion", "potion_5"));
    CHECK(call(0x7000003, {std::string{"potion"}, std::string{"potion_5"}}, services) ==
          Value{double{0.0}});
    CHECK(runtime.stage_completed("potion", "potion_5"));
    CHECK(call(0x7000000, {std::string{"potion"}}, services) == Value{double{0.0}});
    CHECK(runtime.quest_completed("potion"));
    CHECK(call(0x7000004, {std::string{"story"}}, services) == Value{double{0.0}});
    CHECK(runtime.quest_storyline("story"));
}

TEST_CASE("quest natives reject non-string arguments") {
    QuestRuntime runtime;
    NativeServices services;
    services.quest = runtime.access();
    CHECK(call(0x7000001, {std::int32_t{7}, std::int32_t{2}}, services) ==
          Value{double{0.0}});
    CHECK_FALSE(runtime.stage_enabled("7", "2"));
}

TEST_CASE("quest natives fail without a runtime") {
    NativeServices services;
    CHECK(call(0x7000000, {std::string{"potion"}}, services) == Value{double{0.0}});
    CHECK(call(0x7000001, {std::string{"potion"}, std::string{"potion_4"}}, services) ==
          Value{double{0.0}});
}

TEST_CASE("quest native registry entries match the server") {
    const std::pair<std::uint32_t, std::uint8_t> expected[] = {{0x7000000, 1},
                                                               {0x7000001, 2},
                                                               {0x7000002, 2},
                                                               {0x7000003, 2},
                                                               {0x7000004, 1}};
    for (const auto &[id, arity] : expected) {
        const auto *info = NativeRegistry::find(id);
        REQUIRE(info != nullptr);
        CHECK(info->support == gl::script::NativeSupport::supported);
        CHECK(info->arity == arity);
    }
}

TEST_CASE("time-event natives remain unsupported") {
    CHECK(NativeRegistry::find(0x7000005)->support ==
          gl::script::NativeSupport::unsupported);
    CHECK(NativeRegistry::find(0x7000006)->support ==
          gl::script::NativeSupport::unsupported);
    CHECK(NativeRegistry::find(0x7000007)->support ==
          gl::script::NativeSupport::unsupported);
}
