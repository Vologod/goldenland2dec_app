#define SDL_MAIN_HANDLED
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <gl/app/dialog_overlay.hpp>

#include <optional>

int main(int argc, char **argv) {
    doctest::Context context(argc, argv);
    return context.run();
}

TEST_CASE("dialog overlay hit-tests answers") {
    gl::app::DialogOverlay o;
    o.set_content("Стражник", "Больше не подходи ко мне!", {"Уйти", "Спросить"});
    REQUIRE(o.active());
    CHECK(o.hit_test(80, 600) == std::optional<std::size_t>{0});
    CHECK(o.hit_test(80, 660) == std::optional<std::size_t>{1});
}

TEST_CASE("dialog overlay is inactive until content is set") {
    gl::app::DialogOverlay o;
    CHECK_FALSE(o.active());
    CHECK(o.hit_test(80, 600) == std::nullopt);
    o.set_content("Стражник", "Текст", {"Уйти"});
    CHECK(o.active());
    o.clear();
    CHECK_FALSE(o.active());
    CHECK(o.hit_test(80, 600) == std::nullopt);
}

TEST_CASE("dialog overlay hit-test ignores points outside the answer rows") {
    gl::app::DialogOverlay o;
    o.set_content("Стражник", "Текст", {"Уйти", "Спросить"});
    CHECK(o.hit_test(8, 600) == std::nullopt);
    CHECK(o.hit_test(80, 500) == std::nullopt);
    CHECK(o.hit_test(80, 700) == std::nullopt);
}

TEST_CASE("dialog overlay answer rows are 36 px apart") {
    gl::app::DialogOverlay o;
    o.set_content("Стражник", "Текст", {"Уйти", "Спросить"});
    const SDL_FRect first = o.answer_rect(0);
    const SDL_FRect second = o.answer_rect(1);
    CHECK(second.y - first.y == doctest::Approx(36.0f));
    CHECK(first.h > 0.0f);
    CHECK(second.h > 0.0f);
}

TEST_CASE("dialog overlay selection wraps in both directions") {
    gl::app::DialogOverlay o;
    o.set_content("Стражник", "Текст", {"Уйти", "Спросить"});
    CHECK(o.selected() == 0);
    o.move_selection(1);
    CHECK(o.selected() == 1);
    o.move_selection(1);
    CHECK(o.selected() == 0);
    o.move_selection(-1);
    CHECK(o.selected() == 1);
    o.set_content("Стражник", "Текст", {});
    o.move_selection(1);
    CHECK(o.selected() == 0);
}

TEST_CASE("dialog overlay uses a custom viewport for hit-testing") {
    gl::app::DialogOverlay o;
    gl::app::DialogOverlayStyle style;
    style.viewport_width = 640;
    style.viewport_height = 480;
    o.set_style(style);
    o.set_content("Стражник", "Текст", {"Уйти", "Спросить"});
    const SDL_FRect first = o.answer_rect(0);
    const SDL_FRect second = o.answer_rect(1);
    CHECK(o.hit_test(80, static_cast<int>(first.y + 2.0f)) == std::optional<std::size_t>{0});
    CHECK(o.hit_test(80, static_cast<int>(second.y + 2.0f)) == std::optional<std::size_t>{1});
}
