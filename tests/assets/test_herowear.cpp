#include "gl/assets/bytes.hpp"
#include "gl/assets/herowear.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
Bytes text_bytes(std::string_view text) {
    return Bytes(text.begin(), text.end());
}

const HeroWearEntry* find_tech(const HeroWearCatalog& catalog, std::string_view tech) {
    return catalog.find(tech);
}
} // namespace

TEST_CASE("parse_herowear maps TECH to category, group and base set") {
    const std::string text = "category weapon\r\n"
                             "\r\n"
                             "group \"Axe_goluboy2\" \"two_axes_maces\"\r\n"
                             "item \"AXE_1_13_2\"\r\n"
                             "\r\n"
                             "category armor\r\n"
                             "group \"Dospeh_almaz\"\r\n"
                             "item \"ARM_1_13_1\"\r\n"
                             "category shield\r\n"
                             "group \"Shit_bio\"\r\n"
                             "item \"SHL_1_10_1\"\r\n";

    auto catalog = parse_herowear(text_bytes(text));
    REQUIRE(catalog.has_value());
    REQUIRE(catalog->entries.size() == 3);

    const HeroWearEntry* axe = find_tech(catalog.value(), "AXE_1_13_2");
    REQUIRE(axe != nullptr);
    CHECK(axe->category == "weapon");
    CHECK(axe->group == "Axe_goluboy2");
    CHECK(axe->base_set == "two_axes_maces");

    const HeroWearEntry* armor = find_tech(catalog.value(), "ARM_1_13_1");
    REQUIRE(armor != nullptr);
    CHECK(armor->category == "armor");
    CHECK(armor->group == "Dospeh_almaz");
    CHECK(armor->base_set.empty());

    const HeroWearEntry* shield = find_tech(catalog.value(), "SHL_1_10_1");
    REQUIRE(shield != nullptr);
    CHECK(shield->category == "shield");
    CHECK(shield->group == "Shit_bio");
    CHECK(shield->base_set.empty());
}

TEST_CASE("parse_herowear keeps file order and reuses duplicate group names") {
    const std::string text = "category helmet\n"
                             "group \"Shlem_goluboy\"\n"
                             "item \"HLM_1_11_1\"\n"
                             "item \"HLM_1_12_1\"\n"
                             "group \"Shlem_platina\"\n"
                             "item \"HLM_1_04_1\"\n"
                             "group \"Shlem_goluboy\"\n"
                             "item \"HLM_1_13_1\"\n"
                             "category weapon\n"
                             "group \"Axe_goluboy2\" \"two_axes_maces\"\n"
                             "item \"AXE_1_13_2\"\n"
                             "group \"Axe_other\" \"bows\"\n"
                             "item \"AXE_1_13_3\"\n"
                             "group \"Axe_goluboy2\" \"bows\"\n"
                             "item \"AXE_1_13_4\"\n";

    auto catalog = parse_herowear(text_bytes(text));
    REQUIRE(catalog.has_value());
    REQUIRE(catalog->entries.size() == 7);
    CHECK(catalog->entries[0].tech == "HLM_1_11_1");
    CHECK(catalog->entries[0].group == "Shlem_goluboy");
    CHECK(catalog->entries[1].tech == "HLM_1_12_1");
    CHECK(catalog->entries[2].group == "Shlem_platina");
    CHECK(catalog->entries[3].group == "Shlem_goluboy");
    CHECK(catalog->entries[4].base_set == "two_axes_maces");
    CHECK(catalog->entries[5].group == "Axe_other");
    CHECK(catalog->entries[5].base_set == "bows");
    CHECK(catalog->entries[6].group == "Axe_goluboy2");
    CHECK(catalog->entries[6].base_set == "two_axes_maces");
}

TEST_CASE("parse_herowear ignores comments and whitespace") {
    const std::string text = "// header\n"
                             "  category weapon  \n"
                             "\tgroup \"Sword_platina\" \"onehand_wpn\"\n"
                             "item \"SWR_1_08_1\" // trailing\n";

    auto catalog = parse_herowear(text_bytes(text));
    REQUIRE(catalog.has_value());
    REQUIRE(catalog->entries.size() == 1);
    CHECK(catalog->entries[0].tech == "SWR_1_08_1");
    CHECK(catalog->entries[0].base_set == "onehand_wpn");
}

TEST_CASE("parse_herowear rejects an unterminated quoted string") {
    auto catalog = parse_herowear(text_bytes("category weapon\ngroup \"Axe\n"));
    REQUIRE_FALSE(catalog.has_value());
    CHECK(catalog.error().code == gl::ErrorCode::format);
}

TEST_CASE("parse_herowear rejects an item outside a group") {
    auto catalog = parse_herowear(text_bytes("category weapon\nitem \"AXE_1_13_2\"\n"));
    REQUIRE_FALSE(catalog.has_value());
    CHECK(catalog.error().code == gl::ErrorCode::format);
}

TEST_CASE("parse_herowear rejects an unknown directive") {
    auto catalog = parse_herowear(text_bytes("category weapon\nbogus \"x\"\n"));
    REQUIRE_FALSE(catalog.has_value());
    CHECK(catalog.error().code == gl::ErrorCode::format);
}

TEST_CASE("parse_herowear of an empty text is an empty catalog") {
    auto catalog = parse_herowear(Bytes{});
    REQUIRE(catalog.has_value());
    CHECK(catalog->entries.empty());
}
