#include "gl/assets/inv_table.hpp"
#include "gl/assets/cp1251.hpp"
#include "gl/assets/log.hpp"
#include "gl/assets/vfs.hpp"

#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gl::assets {
namespace {
struct Token {
    std::string text;
    bool quoted = false;
};

bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool ci_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i];
        char y = b[i];
        if (x >= 'A' && x <= 'Z')
            x = static_cast<char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z')
            y = static_cast<char>(y - 'A' + 'a');
        if (x != y)
            return false;
    }
    return true;
}

Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

Result<std::vector<Token>> lex(std::span<const std::uint8_t> data) {
    std::vector<Token> tokens;
    const std::size_t size = data.size();
    std::size_t i = 0;
    while (i < size) {
        const unsigned char c = data[i];
        if (c <= 0x20) {
            ++i;
            continue;
        }
        if (c == '/' && i + 1 < size && data[i + 1] == '/') {
            while (i < size && data[i] != '\n')
                ++i;
            if (i < size)
                ++i;
            continue;
        }
        if (c == '"') {
            ++i;
            std::string text;
            bool closed = false;
            while (i < size) {
                const char ch = static_cast<char>(data[i++]);
                if (ch == '"') {
                    closed = true;
                    break;
                }
                text.push_back(ch);
            }
            if (!closed)
                return format_error("inv: unterminated quoted string");
            tokens.push_back(Token{std::move(text), true});
            continue;
        }

        std::string text;
        while (i < size) {
            const unsigned char b = data[i];
            if (b <= 0x20)
                break;
            text.push_back(static_cast<char>(b));
            ++i;
            if (text.size() >= 2 && text[text.size() - 2] == '/' && text.back() == '/') {
                while (i < size && data[i] != '\n')
                    ++i;
                if (i < size)
                    ++i;
            }
        }
        tokens.push_back(Token{std::move(text), false});
    }
    return tokens;
}

bool parse_double_token(const Token &token, double &out) {
    if (token.quoted || token.text.empty())
        return false;
    for (char c : token.text) {
        if (!((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.'))
            return false;
    }
    const char *begin = token.text.c_str();
    char *end = nullptr;
    const double value = std::strtod(begin, &end);
    if (static_cast<std::size_t>(end - begin) != token.text.size())
        return false;
    out = value;
    return true;
}

bool parse_int_token(const Token &token, std::int32_t &out) {
    if (token.quoted || token.text.empty())
        return false;
    const std::string &text = token.text;
    if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        if (text.size() == 2)
            return false;
        for (std::size_t i = 2; i < text.size(); ++i) {
            if (!is_hex_digit(text[i]))
                return false;
        }
        out = static_cast<std::int32_t>(std::strtoul(text.c_str(), nullptr, 16));
        return true;
    }
    for (char c : text) {
        if (!((c >= '0' && c <= '9') || c == '-'))
            return false;
    }
    out = static_cast<std::int32_t>(std::strtol(text.c_str(), nullptr, 10));
    return true;
}

bool find_header(const std::vector<Token> &tokens, std::string_view key, double &out) {
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].quoted || !ci_equal(tokens[i].text, key))
            continue;
        if (i + 1 < tokens.size() && parse_double_token(tokens[i + 1], out))
            return true;
    }
    return false;
}
} // namespace

Result<InvTable> parse_inv(std::span<const std::uint8_t> data) {
    auto tokens = lex(data);
    if (!tokens)
        return tokens.error();

    InvTable table;
    double value = 0.0;
    if (find_header(tokens.value(), "level_offset", value))
        table.level_offset = static_cast<std::int32_t>(value);
    if (find_header(tokens.value(), "regenerate_chance", value))
        table.regenerate_chance = value * 0.01;
    if (find_header(tokens.value(), "chance_decrease", value))
        table.chance_decrease = value;
    if (find_header(tokens.value(), "quantity_decrease", value))
        table.quantity_decrease = value;

    const std::vector<Token> &list = tokens.value();
    std::size_t skipped_four = 0;
    std::size_t i = 0;
    while (i < list.size()) {
        if (list[i].quoted || !ci_equal(list[i].text, "item")) {
            ++i;
            continue;
        }

        std::size_t j = i + 1;
        if (j >= list.size() || !list[j].quoted) {
            i = j;
            continue;
        }
        std::string name = list[j].text;
        ++j;

        double n1 = 0.0;
        double n2 = 0.0;
        double n3 = 0.0;
        std::int32_t n4 = 0;
        std::int32_t n5 = 0;
        if (j >= list.size() || !parse_double_token(list[j], n1)) {
            i = j;
            continue;
        }
        ++j;
        if (j >= list.size() || !parse_double_token(list[j], n2)) {
            i = j;
            continue;
        }
        ++j;
        if (j >= list.size() || !parse_double_token(list[j], n3)) {
            i = j;
            continue;
        }
        ++j;
        if (j >= list.size() || !parse_int_token(list[j], n4)) {
            i = j;
            continue;
        }
        ++j;
        if (j >= list.size() || !parse_int_token(list[j], n5)) {
            ++skipped_four;
            i = j;
            continue;
        }
        ++j;

        table.items.push_back(InvItem{std::move(name), static_cast<std::int32_t>(n1),
                                      static_cast<std::int32_t>(n2), n3, n4, n5});
        i = j;
    }

    if (skipped_four != 0)
        log(LogLevel::warning,
            "inv: skipped " + std::to_string(skipped_four) + " item record(s) with 4 numbers");

    return table;
}

Result<InvTable> load_inv(GameVfs &vfs, std::string_view name) {
    const std::string path =
        normalize_virtual_path("scripts/inventory/" + std::string(name) + ".inv");
    auto bytes = vfs.read(path);
    if (!bytes)
        return Error{bytes.error().code,
                     "inv: cannot read '" + path + "': " + bytes.error().message};
    auto table = parse_inv(bytes.value());
    if (!table)
        return table.error();
    return table;
}
} // namespace gl::assets
