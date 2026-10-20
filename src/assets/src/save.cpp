#include "gl/assets/save.hpp"
#include "gl/assets/byte_reader.hpp"
#include "gl/assets/cp1251.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <new>
#include <string>

namespace gl::assets {
namespace {
constexpr std::size_t kHeaderSize = 112;
constexpr std::size_t kTitleOffset = 40;
constexpr std::size_t kTitleSize = 64;
constexpr std::array<std::uint8_t, 20> kMagic = {
    'G', 'o', 'l', 'd', 'e', 'n', 'l', 'a', 'n', 'd', ' ', 'I', 'I', ' ', 's', 'a', 'v', 'e', ' ',
    '\0'};

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

bool is_title_padding(std::uint8_t byte) { return byte == 0 || byte == ' '; }
}

Result<SaveHeader> parse_save_header(std::span<const std::uint8_t> data) {
    try {
        if (data.size() < kHeaderSize)
            return format_error("save data is shorter than the 112-byte header");
        if (!std::equal(kMagic.begin(), kMagic.end(), data.begin()))
            return format_error("bad save file magic");

        SaveHeader header;
        ByteReader reader(data);
        reader.skip(kMagic.size());
        if (!reader.read_u32le(header.version) || !reader.read_u16le(header.year) ||
            !reader.read_u16le(header.month) || !reader.skip(2) ||
            !reader.read_u16le(header.day) || !reader.read_u16le(header.time_a) ||
            !reader.read_u16le(header.time_b) || !reader.read_u16le(header.time_c)) {
            return format_error("truncated save header");
        }

        std::size_t length = 0;
        while (length < kTitleSize && data[kTitleOffset + length] != 0) ++length;
        while (length > 0 && is_title_padding(data[kTitleOffset + length - 1])) --length;

        header.title = cp1251_to_utf8(data.subspan(kTitleOffset, length));
        return header;
    } catch (const std::bad_alloc&) {
        return format_error("save header allocation failed");
    }
}
}
