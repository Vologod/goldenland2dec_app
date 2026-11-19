#include "gl/game/quest_runtime.hpp"

namespace gl::game {
void QuestRuntime::enable_quest(std::string_view id, bool storyline) {
    if (id.empty()) return;
    QuestState &state = quests_[std::string(id)];
    state.enabled = true;
    state.storyline = state.storyline || storyline;
}

void QuestRuntime::complete_quest(std::string_view id) {
    if (id.empty()) return;
    QuestState &state = quests_[std::string(id)];
    state.enabled = true;
    state.completed = true;
}

bool QuestRuntime::quest_enabled(std::string_view id) const {
    const auto it = quests_.find(std::string(id));
    return it != quests_.end() && it->second.enabled;
}

bool QuestRuntime::quest_completed(std::string_view id) const {
    const auto it = quests_.find(std::string(id));
    return it != quests_.end() && it->second.completed;
}

bool QuestRuntime::quest_storyline(std::string_view id) const {
    const auto it = quests_.find(std::string(id));
    return it != quests_.end() && it->second.storyline;
}

void QuestRuntime::enable_stage(std::string_view quest, std::string_view stage) {
    if (quest.empty() || stage.empty()) return;
    stages_[{std::string(quest), std::string(stage)}].enabled = true;
}

void QuestRuntime::complete_stage(std::string_view quest, std::string_view stage) {
    if (quest.empty() || stage.empty()) return;
    StageState &state = stages_[{std::string(quest), std::string(stage)}];
    state.enabled = true;
    state.completed = true;
}

bool QuestRuntime::stage_enabled(std::string_view quest, std::string_view stage) const {
    const auto it = stages_.find({std::string(quest), std::string(stage)});
    return it != stages_.end() && it->second.enabled;
}

bool QuestRuntime::stage_completed(std::string_view quest, std::string_view stage) const {
    const auto it = stages_.find({std::string(quest), std::string(stage)});
    return it != stages_.end() && it->second.completed;
}

void QuestRuntime::clear() {
    quests_.clear();
    stages_.clear();
}

gl::script::QuestAccess QuestRuntime::access() {
    gl::script::QuestAccess access;
    access.enable_quest = [this](std::string_view id, bool storyline) {
        enable_quest(id, storyline);
    };
    access.complete_quest = [this](std::string_view id) { complete_quest(id); };
    access.enable_stage = [this](std::string_view quest, std::string_view stage) {
        enable_stage(quest, stage);
    };
    access.complete_stage = [this](std::string_view quest, std::string_view stage) {
        complete_stage(quest, stage);
    };
    return access;
}
} // namespace gl::game
