#include "gl/script/dialog_session.hpp"
#include <string>
#include <utility>
#include <vector>

namespace gl::script {
DialogSession::DialogSession(ScriptVm &vm, const gl::assets::Sdb &sdb) : vm_(vm), sdb_(sdb) {}

gl::Result<void> DialogSession::start() {
    vm_.restart(true);
    run_to_pause();
    refresh();
    return {};
}

bool DialogSession::waiting() const {
    return vm_.status() == VmStatus::wait_user && dialog_available_;
}

std::string_view DialogSession::npc_text() const {
    if (!waiting())
        return {};
    return npc_text_;
}

std::span<const DialogLine> DialogSession::answers() const {
    if (!waiting())
        return {};
    return answers_;
}

gl::Result<void> DialogSession::choose(std::size_t index) {
    if (!waiting())
        return gl::Error{gl::ErrorCode::argument, "dialog is not waiting"};
    if (index >= answers_.size())
        return gl::Error{gl::ErrorCode::argument, "answer index out of range"};
    if (!vm_.user_input(index))
        return gl::Error{gl::ErrorCode::argument, "answer index out of range"};

    run_to_pause();
    refresh();
    return {};
}

VmStatus DialogSession::status() const { return vm_.status(); }

std::string DialogSession::phrase_text(std::uint32_t id) const {
    if (const std::string *text = sdb_.find(id))
        return *text;
    return "<phrase " + std::to_string(id) + ">";
}

void DialogSession::run_to_pause() {
    while (vm_.step()) {
    }
}

void DialogSession::refresh() {
    npc_text_.clear();
    answers_.clear();
    dialog_available_ = false;

    const std::vector<std::uint32_t> data = vm_.dialogs_data();
    if (data.empty())
        return;

    dialog_available_ = true;
    npc_text_ = phrase_text(data.front());
    answers_.reserve(data.size() - 1);
    for (std::size_t i = 1; i < data.size(); ++i) {
        answers_.push_back(DialogLine{data[i], phrase_text(data[i])});
    }
}
} // namespace gl::script
