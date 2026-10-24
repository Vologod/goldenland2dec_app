#pragma once

namespace gl::app {

inline bool level_input_enabled(bool dialog_active, bool save_slots_active,
                                bool console_active) {
    return !dialog_active && !save_slots_active && !console_active;
}

} // namespace gl::app
