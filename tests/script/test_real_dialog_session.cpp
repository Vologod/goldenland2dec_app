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
#include <string_view>
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

bool has_cyrillic_utf8(std::string_view text) {
    bool cyrillic = false;
    std::size_t index = 0;
    while (index < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        std::size_t length = 0;
        char32_t codepoint = 0;
        if (lead < 0x80) {
            codepoint = lead;
            length = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            codepoint = lead & 0x1F;
            length = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            codepoint = lead & 0x0F;
            length = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            codepoint = lead & 0x07;
            length = 4;
        } else {
            return false;
        }
        if (index + length > text.size()) return false;
        for (std::size_t offset = 1; offset < length; ++offset) {
            const unsigned char continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0) != 0x80) return false;
            codepoint = (codepoint << 6) | (continuation & 0x3F);
        }
        if (codepoint >= 0x0400 && codepoint <= 0x04FF) cyrillic = true;
        index += length;
    }
    return cyrillic;
}
} // namespace

TEST_CASE("real data: dialog session maps demon phrases and advances" *
          doctest::skip(!game_dir_available())) {
    const char* dir = std::getenv("GL_GAME_DIR");
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
    CHECK_FALSE(vm.failed());
    CHECK_FALSE(session.failed());
    REQUIRE(session.status() == VmStatus::wait_user);
    REQUIRE(session.waiting());

    const std::string_view npc = session.npc_text();
    CHECK_FALSE(npc.empty());
    CHECK(npc.rfind("<phrase ", 0) != 0);
    CHECK(has_cyrillic_utf8(npc));

    const std::span<const gl::script::DialogLine> answers = session.answers();
    REQUIRE(answers.size() >= 1);
    for (const gl::script::DialogLine& line : answers) {
        CHECK(line.phrase_id > 0);
        CHECK_FALSE(line.text.empty());
    }

    auto chosen = session.choose(0);
    REQUIRE(chosen.has_value());
    CHECK_FALSE(vm.failed());
    CHECK_FALSE(session.failed());
    CHECK((session.status() == VmStatus::wait_user || session.status() == VmStatus::end));
    if (session.waiting()) CHECK_FALSE(session.npc_text().empty());
}
