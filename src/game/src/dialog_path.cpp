#include "gl/game/dialog_path.hpp"

namespace gl::game {
std::optional<std::string> dialog_script_path(const PersonInstance& person) {
    if (person.dialog.empty()) return std::nullopt;
    std::string p = person.dialog;
    for (char& c : p)
        if (c == '\\') c = '/';
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    std::string out = "scripts/dialogs/" + p;
    if (out.size() < 3 || out.substr(out.size() - 3) != ".cs") out += ".cs";
    return out;
}
} // namespace gl::game
