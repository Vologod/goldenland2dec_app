#include "gl/game/party_runtime.hpp"
#include "gl/game/person_runtime.hpp"

#include <algorithm>

namespace gl::game {
std::int32_t PartyRuntime::add(std::string_view name) {
    if (members_.size() >= kMaxParty) return -1;
    members_.emplace_back(name);
    if (persons_ != nullptr && persons_->has_person(name)) persons_->set_party_role(name, 1);
    return static_cast<std::int32_t>(kMaxParty - members_.size());
}

std::int32_t PartyRuntime::remove(std::string_view name) {
    if (members_.empty()) return static_cast<std::int32_t>(kMaxParty);
    if (persons_ != nullptr && persons_->has_person(name)) persons_->set_party_role(name, 2);
    const auto it = std::find(members_.begin(), members_.end(), name);
    if (it != members_.end()) members_.erase(it);
    return static_cast<std::int32_t>(kMaxParty - members_.size());
}

void PartyRuntime::clear() {
    if (persons_ != nullptr) {
        for (const std::string &name : members_)
            if (persons_->has_person(name)) persons_->set_party_role(name, 0);
    }
    members_.clear();
}

void PartyRuntime::attach(PersonRuntime *persons) {
    persons_ = persons;
    if (persons_ == nullptr) return;
    for (const std::string &name : members_)
        if (persons_->has_person(name)) persons_->set_party_role(name, 1);
}

bool PartyRuntime::has(std::string_view name) const {
    return std::find(members_.begin(), members_.end(), name) != members_.end();
}

gl::script::PartyAccess PartyRuntime::access() {
    gl::script::PartyAccess access;
    access.add = [this](std::string_view name) { return add(name); };
    access.remove = [this](std::string_view name) { return remove(name); };
    access.has = [this](std::string_view name) { return has(name); };
    return access;
}
} // namespace gl::game
