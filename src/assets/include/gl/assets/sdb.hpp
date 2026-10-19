#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gl::assets {
struct SdbEntry { std::uint32_t id = 0; std::string value; };

class Sdb {
public:
    static Result<Sdb> parse(std::span<const std::uint8_t> data);
    const std::string* find(std::uint32_t id) const;
    const std::vector<SdbEntry>& entries() const { return entries_; }
    bool had_signature() const { return had_signature_; }
private:
    bool had_signature_ = false;
    std::vector<SdbEntry> entries_;
};
}
