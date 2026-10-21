#include "gl/render/utf8.hpp"

#include <cstddef>
#include <cstdint>

namespace gl::render {
namespace {

constexpr char32_t kReplacement = 0xFFFD;

bool is_continuation(const unsigned char byte) { return (byte & 0xC0u) == 0x80u; }

} // namespace

std::vector<char32_t> utf8_decode(std::string_view utf8) {
    std::vector<char32_t> codepoints;
    codepoints.reserve(utf8.size());

    std::size_t index = 0;
    while (index < utf8.size()) {
        const auto lead = static_cast<unsigned char>(utf8[index]);

        if (lead < 0x80u) {
            codepoints.push_back(static_cast<char32_t>(lead));
            ++index;
            continue;
        }

        if (lead >= 0xC2u && lead <= 0xDFu) {
            if (index + 1 < utf8.size() &&
                is_continuation(static_cast<unsigned char>(utf8[index + 1]))) {
                const auto second = static_cast<unsigned char>(utf8[index + 1]);
                codepoints.push_back(
                    static_cast<char32_t>(((lead & 0x1Fu) << 6) | (second & 0x3Fu)));
                index += 2;
                continue;
            }
            codepoints.push_back(kReplacement);
            ++index;
            continue;
        }

        if (lead >= 0xE0u && lead <= 0xEFu) {
            if (index + 2 < utf8.size()) {
                const auto second = static_cast<unsigned char>(utf8[index + 1]);
                const auto third = static_cast<unsigned char>(utf8[index + 2]);
                const bool second_ok = is_continuation(second) &&
                                       !(lead == 0xE0u && second < 0xA0u) &&
                                       !(lead == 0xEDu && second > 0x9Fu);
                if (second_ok && is_continuation(third)) {
                    codepoints.push_back(static_cast<char32_t>(
                        ((lead & 0x0Fu) << 12) | ((second & 0x3Fu) << 6) | (third & 0x3Fu)));
                    index += 3;
                    continue;
                }
            }
            codepoints.push_back(kReplacement);
            ++index;
            continue;
        }

        if (lead >= 0xF0u && lead <= 0xF4u) {
            if (index + 3 < utf8.size()) {
                const auto second = static_cast<unsigned char>(utf8[index + 1]);
                const auto third = static_cast<unsigned char>(utf8[index + 2]);
                const auto fourth = static_cast<unsigned char>(utf8[index + 3]);
                const bool second_ok = is_continuation(second) &&
                                       !(lead == 0xF0u && second < 0x90u) &&
                                       !(lead == 0xF4u && second > 0x8Fu);
                if (second_ok && is_continuation(third) && is_continuation(fourth)) {
                    codepoints.push_back(
                        static_cast<char32_t>(((lead & 0x07u) << 18) | ((second & 0x3Fu) << 12) |
                                              ((third & 0x3Fu) << 6) | (fourth & 0x3Fu)));
                    index += 4;
                    continue;
                }
            }
            codepoints.push_back(kReplacement);
            ++index;
            continue;
        }

        codepoints.push_back(kReplacement);
        ++index;
    }

    return codepoints;
}

}
