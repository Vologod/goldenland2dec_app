#include "gl/assets/cp1251.hpp"
#include <cstddef>

namespace gl::assets {
namespace {
constexpr char16_t kCp1251[128] = {
    0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,
    0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
    0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
    0x0098,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
    0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,
    0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
    0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,
    0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457,
    0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,
    0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
    0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,
    0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
    0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,
    0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
    0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,
    0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F,
};

void append_utf8(std::string& out, char16_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool is_continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

std::size_t utf8_sequence_length(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    return 4;
}

bool is_valid_utf8(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            ++i;
            continue;
        }
        std::size_t len = 0;
        if (c >= 0xC2 && c <= 0xDF) len = 2;
        else if (c >= 0xE0 && c <= 0xEF) len = 3;
        else if (c >= 0xF0 && c <= 0xF4) len = 4;
        else return false;
        if (i + len > s.size()) return false;
        for (std::size_t k = 1; k < len; ++k)
            if (!is_continuation(static_cast<unsigned char>(s[i + k]))) return false;
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        if (len == 3) {
            if (c == 0xE0 && c1 < 0xA0) return false;
            if (c == 0xED && c1 > 0x9F) return false;
        } else if (len == 4) {
            if (c == 0xF0 && c1 < 0x90) return false;
            if (c == 0xF4 && c1 > 0x8F) return false;
        }
        i += len;
    }
    return true;
}
}

std::string cp1251_to_utf8(std::span<const std::uint8_t> input) {
    std::string out;
    out.reserve(input.size());
    for (std::uint8_t b : input) append_utf8(out, b < 0x80 ? b : kCp1251[b - 0x80]);
    return out;
}

std::string utf8_to_cp1251(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size();) {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
            continue;
        }
        std::size_t len = 0;
        char32_t cp = 0;
        if ((c & 0xE0) == 0xC0) {
            len = 2;
            cp = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
            cp = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
            cp = c & 0x07;
        }
        if (len == 0 || i + len > input.size()) {
            out.push_back('?');
            ++i;
            continue;
        }
        bool valid = true;
        for (std::size_t k = 1; k < len; ++k) {
            const unsigned char b = static_cast<unsigned char>(input[i + k]);
            if (!is_continuation(b)) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (b & 0x3F);
        }
        if (!valid) {
            out.push_back('?');
            ++i;
            continue;
        }
        if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) ||
            (len == 4 && cp < 0x10000) || cp > 0x10FFFF ||
            (cp >= 0xD800 && cp <= 0xDFFF)) {
            out.push_back('?');
            i += len;
            continue;
        }
        char mapped = '?';
        for (std::size_t k = 0; k < 128; ++k) {
            if (static_cast<char32_t>(kCp1251[k]) == cp) {
                mapped = static_cast<char>(0x80 + k);
                break;
            }
        }
        out.push_back(mapped);
        i += len;
    }
    return out;
}

std::string normalize_virtual_path(std::string_view path) {
    const bool utf8 = is_valid_utf8(path);
    std::string out;
    out.reserve(path.size());
    bool last_slash = false;
    for (std::size_t i = 0; i < path.size();) {
        unsigned char c = static_cast<unsigned char>(path[i]);
        if (utf8 && c >= 0x80) {
            const std::size_t len = utf8_sequence_length(c);
            if (c == 0xD0 || c == 0xD1) {
                char16_t cp = static_cast<char16_t>(
                    ((c & 0x1F) << 6) | (static_cast<unsigned char>(path[i + 1]) & 0x3F));
                if (cp >= 0x0410 && cp <= 0x042F) cp = static_cast<char16_t>(cp + 0x20);
                else if (cp == 0x0401) cp = 0x0451;
                append_utf8(out, cp);
            } else {
                out.append(path.data() + i, len);
            }
            last_slash = false;
            i += len;
            continue;
        }
        ++i;
        if (c == '\\') c = '/';
        if (c == '/' && last_slash) continue;
        last_slash = c == '/';
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
        else if (!utf8 && c >= 0xC0 && c <= 0xDF) c = static_cast<unsigned char>(c + 0x20);
        out.push_back(static_cast<char>(c));
    }
    std::size_t begin = 0;
    while (begin < out.size() && out[begin] == '/') ++begin;
    std::size_t end = out.size();
    while (end > begin && out[end - 1] == '/') --end;
    return out.substr(begin, end - begin);
}

bool is_safe_relative_path(std::string_view path) {
    if (path.empty()) return false;
    if (path.front() == '/' || path.front() == '\\') return false;
    if (path.size() >= 2 && path[1] == ':') return false;
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t slash = path.find_first_of("/\\", start);
        std::string_view part = path.substr(start, slash == std::string_view::npos
                                                      ? std::string_view::npos : slash - start);
        if (part == "..") return false;
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return true;
}
}
