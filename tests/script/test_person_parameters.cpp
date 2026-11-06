#include <doctest/doctest.h>
#include "gl/script/native_registry.hpp"
#include "gl/script/person_access.hpp"
#include <string>
#include <vector>

using gl::script::PersonParameter;

TEST_CASE("person parameter names match Server.dll") {
    CHECK(gl::script::person_parameter_from_name("EXPERIENCE") == PersonParameter::experience);
    CHECK(gl::script::person_parameter_from_name("REPUTATION") == PersonParameter::reputation);
    CHECK(gl::script::person_parameter_from_name("INTELLIGENCE") == PersonParameter::intelligence);
    CHECK(gl::script::person_parameter_from_name("WISDOM") == PersonParameter::wisdom);
    CHECK(gl::script::person_parameter_from_name("STRENGTH") == PersonParameter::strength);
    CHECK(gl::script::person_parameter_from_name("CONSTITUTION") == PersonParameter::constitution);
    CHECK(gl::script::person_parameter_from_name("DEXTERITY") == PersonParameter::dexterity);
    CHECK(gl::script::person_parameter_from_name("LUCK") == PersonParameter::luck);
    CHECK(gl::script::person_parameter_from_name("PERCEPTION") == PersonParameter::perception);
    CHECK(gl::script::person_parameter_from_name("MAXIMUM LIFE") == PersonParameter::maximum_life);
    CHECK(gl::script::person_parameter_from_name("LIFE") == PersonParameter::life);
    CHECK(gl::script::person_parameter_from_name("ENERGY") == PersonParameter::energy);
    CHECK(gl::script::person_parameter_from_name("REPUTATIUON") == PersonParameter::unknown);
    CHECK(gl::script::person_parameter_from_name("reputation") == PersonParameter::unknown);
    CHECK(gl::script::person_parameter_from_name("") == PersonParameter::unknown);
}

TEST_CASE("get person parameter dispatches to services") {
    gl::script::NativeServices services;
    std::string seen_person;
    PersonParameter seen_param = PersonParameter::unknown;
    services.person.get_parameter = [&](std::string_view person, PersonParameter param) {
        seen_person = std::string(person);
        seen_param = param;
        return 42.0;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("REPUTATION");
    const gl::script::Value args[] = {person, param};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000000), args, services);
    CHECK(seen_person == "Hero");
    CHECK(seen_param == PersonParameter::reputation);
    CHECK(std::get<double>(result) == doctest::Approx(42.0));
}

TEST_CASE("set person parameter dispatches to services") {
    gl::script::NativeServices services;
    std::string seen_person;
    PersonParameter seen_param = PersonParameter::unknown;
    double seen_value = 0.0;
    services.person.set_parameter = [&](std::string_view person, PersonParameter param, double value) {
        seen_person = std::string(person);
        seen_param = param;
        seen_value = value;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("LIFE");
    const gl::script::Value value = std::int32_t{7};
    const gl::script::Value args[] = {person, param, value};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000001), args, services);
    CHECK(seen_person == "Hero");
    CHECK(seen_param == PersonParameter::life);
    CHECK(seen_value == 7.0);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("set person parameter truncates fractional values to int") {
    gl::script::NativeServices services;
    double seen_value = 0.0;
    services.person.set_parameter = [&](std::string_view, PersonParameter, double value) {
        seen_value = value;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("LIFE");
    const gl::script::Value value = 7.9;
    const gl::script::Value args[] = {person, param, value};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000001), args, services);
    CHECK(seen_value == 7.0);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("set person parameter rejects a non-numeric value") {
    gl::script::NativeServices services;
    bool called = false;
    std::vector<std::string> messages;
    services.person.set_parameter = [&](std::string_view, PersonParameter, double) {
        called = true;
    };
    services.log = [&](std::string_view text) { messages.emplace_back(text); };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("LIFE");
    const gl::script::Value value = std::string("seven");
    const gl::script::Value args[] = {person, param, value};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000001), args, services);
    CHECK_FALSE(called);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("invalid") != std::string::npos);
}

TEST_CASE("person native registry arities match Server.dll") {
    CHECK(gl::script::NativeRegistry::find(0x4000000)->arity == 2);
    CHECK(gl::script::NativeRegistry::find(0x4000001)->arity == 3);
    CHECK(gl::script::NativeRegistry::find(0x400000C)->arity == 2);
}

TEST_CASE("get person skill dispatches to services") {
    gl::script::NativeServices services;
    std::string seen_skill;
    services.person.get_skill = [&](std::string_view, std::string_view skill) {
        seen_skill = std::string(skill);
        return 3.0;
    };
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value skill = std::string("SKILL_WPN_AXE");
    const gl::script::Value args[] = {person, skill};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x400000C), args, services);
    CHECK(seen_skill == "SKILL_WPN_AXE");
    CHECK(std::get<double>(result) == doctest::Approx(3.0));
}

TEST_CASE("person native without services returns zero") {
    gl::script::NativeServices services;
    const gl::script::Value person = std::string("Hero");
    const gl::script::Value param = std::string("REPUTATION");
    const gl::script::Value args[] = {person, param};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000000), args, services);
    CHECK(std::get<double>(result) == doctest::Approx(0.0));
}

TEST_CASE("person native rejects wrong arguments") {
    gl::script::NativeServices services;
    const gl::script::Value only = std::string("Hero");
    const gl::script::Value args[] = {only};
    const auto result = gl::script::NativeRegistry::call(
        *gl::script::NativeRegistry::find(0x4000000), args, services);
    CHECK(std::get<std::int32_t>(result) == 0);
}
