#pragma once
#include <functional>
#include <string_view>

namespace gl::script {
struct QuestAccess {
    std::function<void(std::string_view, bool)> enable_quest;
    std::function<void(std::string_view)> complete_quest;
    std::function<void(std::string_view, std::string_view)> enable_stage;
    std::function<void(std::string_view, std::string_view)> complete_stage;
};
} // namespace gl::script
