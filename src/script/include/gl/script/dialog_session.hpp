#pragma once
#include "gl/assets/result.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/script/script_vm.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gl::script {
struct DialogLine {
    std::uint32_t phrase_id = 0;
    std::string text;
};

class DialogSession {
public:
    DialogSession(ScriptVm& vm, const gl::assets::Sdb& sdb);
    // Restarts the VM from node 0 (`restart(true)`): variables are rebuilt from globals and
    // previously seeded runtime state is discarded.
    gl::Result<void> start();
    bool waiting() const;
    std::string_view npc_text() const;
    std::span<const DialogLine> answers() const;
    // Maps VM failure after running to `ErrorCode::format`; `status()`/`failed()` stay available.
    gl::Result<void> choose(std::size_t index);
    VmStatus status() const;

private:
    std::string phrase_text(std::uint32_t id) const;
    void run_to_pause();
    void refresh();

    ScriptVm& vm_;
    const gl::assets::Sdb& sdb_;
    std::string npc_text_;
    std::vector<DialogLine> answers_;
};
}
