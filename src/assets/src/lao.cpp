#include "gl/assets/lao.hpp"
#include "gl/assets/byte_reader.hpp"
#include <new>
#include <string>

namespace gl::assets {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }
}

Result<std::vector<LaoInfo>> parse_lao(std::span<const std::uint8_t> data) {
    if (data.size() % 8 != 0) {
        return format_error("LAO size is not a multiple of 8: " + std::to_string(data.size()));
    }
    try {
        std::vector<LaoInfo> entries;
        entries.reserve(data.size() / 8);
        ByteReader reader(data);
        while (!reader.at_end()) {
            LaoInfo entry;
            const std::size_t offset = reader.position();
            if (!reader.read_u32le(entry.height) || !reader.read_u32le(entry.delay)) {
                return format_error("truncated LAO entry at offset " + std::to_string(offset));
            }
            entries.push_back(entry);
        }
        return entries;
    } catch (const std::bad_alloc&) {
        return format_error("LAO allocation failed");
    }
}
}
