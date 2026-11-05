#pragma once

namespace gl::app {

inline bool level_input_enabled(bool dialog_active, bool save_slots_active,
                                bool console_active, bool inventory_active = false) {
    return !dialog_active && !save_slots_active && !console_active && !inventory_active;
}

} // namespace gl::app
