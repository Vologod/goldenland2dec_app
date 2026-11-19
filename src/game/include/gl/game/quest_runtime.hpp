#pragma once
#include "gl/script/quest_access.hpp"

#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace gl::game {
struct QuestState {
    bool enabled = false;
    bool completed = false;
    bool storyline = false;
};

struct StageState {
    bool enabled = false;
    bool completed = false;
};

class QuestRuntime {
  public:
    void enable_quest(std::string_view id, bool storyline = false);
    void complete_quest(std::string_view id);
    bool quest_enabled(std::string_view id) const;
    bool quest_completed(std::string_view id) const;
    bool quest_storyline(std::string_view id) const;

    void enable_stage(std::string_view quest, std::string_view stage);
    void complete_stage(std::string_view quest, std::string_view stage);
    bool stage_enabled(std::string_view quest, std::string_view stage) const;
    bool stage_completed(std::string_view quest, std::string_view stage) const;

    void clear();

    gl::script::QuestAccess access();

  private:
    using StageKey = std::pair<std::string, std::string>;
    std::map<std::string, QuestState> quests_;
    std::map<StageKey, StageState> stages_;
};
} // namespace gl::game
