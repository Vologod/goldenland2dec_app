#include <doctest/doctest.h>
#include "gl/game/dialog_path.hpp"

TEST_CASE("dialog path maps scr_dialog") {
    gl::game::PersonInstance p;
    p.dialog = "l1_1\\l1.p621_svet_3circusworkerv.d621.age";
    CHECK(gl::game::dialog_script_path(p) ==
          std::optional<std::string>{"scripts/dialogs/l1_1/l1.p621_svet_3circusworkerv.d621.age.cs"});
}

TEST_CASE("empty dialog is not talkable") {
    gl::game::PersonInstance p;
    CHECK_FALSE(gl::game::dialog_script_path(p));
}

TEST_CASE("existing .cs is not duplicated") {
    gl::game::PersonInstance p;
    p.dialog = "demon.d1.age.cs";
    CHECK(gl::game::dialog_script_path(p) ==
          std::optional<std::string>{"scripts/dialogs/demon.d1.age.cs"});
}
