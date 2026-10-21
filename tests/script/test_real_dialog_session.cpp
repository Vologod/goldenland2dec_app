#include "gl/assets/cs.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/script/dialog_session.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/host.hpp"
#include "gl/script/script_vm.hpp"
#include <cstdlib>
#include <doctest/doctest.h>
#include <span>
#include <utility>

using gl::assets::GameVfs;
using gl::assets::parse_cs;
using gl::assets::Sdb;
using gl::script::DefaultScriptHost;
using gl::script::DialogSession;
using gl::script::parse_globals;
using gl::script::ScriptVm;
using gl::script::VmStatus;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("real data: dialog session maps demon phrases and advances" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);

    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto script_data = vfs->read("scripts/dialogs/demon.d1.age.cs");
    REQUIRE(script_data.has_value());
    auto nodes = parse_cs(script_data.value());
    REQUIRE(nodes.has_value());

    auto globals_data = vfs->read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(globals_data.has_value());
    auto globals = parse_globals(globals_data.value());
    REQUIRE(globals.has_value());

    auto sdb_data = vfs->read("sdb/dialogs/dialogsphrases.sdb");
    REQUIRE(sdb_data.has_value());
    auto sdb = Sdb::parse(sdb_data.value());
    REQUIRE(sdb.has_value());

    DefaultScriptHost host;
    ScriptVm vm(std::move(nodes.value()), host);
    vm.set_globals(globals.value());

    DialogSession session(vm, sdb.value());
    auto started = session.start();
    REQUIRE(started.has_value());
    REQUIRE(session.status() == VmStatus::wait_user);
    REQUIRE(session.waiting());

    const std::string_view npc = session.npc_text();
    CHECK_FALSE(npc.empty());
    CHECK(npc.rfind("<phrase ", 0) != 0);

    const std::span<const gl::script::DialogLine> answers = session.answers();
    REQUIRE(answers.size() >= 1);
    for (const gl::script::DialogLine &line : answers) {
        CHECK(line.phrase_id > 0);
        CHECK_FALSE(line.text.empty());
    }

    auto chosen = session.choose(0);
    REQUIRE(chosen.has_value());
    CHECK((session.status() == VmStatus::wait_user || session.status() == VmStatus::end ||
           session.status() == VmStatus::infinity));
    if (session.waiting())
        CHECK_FALSE(session.npc_text().empty());
}
