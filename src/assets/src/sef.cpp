#include "gl/assets/sef.hpp"
#include "gl/assets/cp1251.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace gl::assets {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }

Error format_error_at(std::string message, std::size_t line) {
    return format_error(std::move(message) + " on line " + std::to_string(line));
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f' ||
           c == '\0';
}

std::string ascii_lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string to_utf8(std::string_view text) {
    return cp1251_to_utf8({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

bool parse_int(std::string_view value, int& out) {
    if (value.empty())
        return false;
    int parsed = 0;
    const auto [end, error] =
        std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size())
        return false;
    out = parsed;
    return true;
}

enum class TokenKind { word, string, lbrace, rbrace, colon, semicolon, end };

struct Token {
    TokenKind kind = TokenKind::end;
    std::string text;
    std::size_t line = 1;
};

class Lexer {
public:
    explicit Lexer(std::string_view text) : text_(text) {
        if (text_.starts_with("\xEF\xBB\xBF"))
            text_.remove_prefix(3);
    }

    Token next() {
        skip_ignored();
        if (position_ >= text_.size())
            return {TokenKind::end, {}, line_};

        const char c = text_[position_];
        switch (c) {
        case '{':
            ++position_;
            return {TokenKind::lbrace, {}, line_};
        case '}':
            ++position_;
            return {TokenKind::rbrace, {}, line_};
        case ':':
            ++position_;
            return {TokenKind::colon, {}, line_};
        case ';':
            ++position_;
            return {TokenKind::semicolon, {}, line_};
        case '"':
            return read_string();
        default:
            return read_word();
        }
    }

    bool failed() const { return failed_; }

    void skip_balanced() {
        int depth = 1;
        while (depth > 0) {
            const Token token = next();
            if (failed_)
                return;
            if (token.kind == TokenKind::end) {
                failed_ = true;
                return;
            }
            if (token.kind == TokenKind::lbrace)
                ++depth;
            else if (token.kind == TokenKind::rbrace)
                --depth;
        }
    }

private:
    void skip_ignored() {
        while (position_ < text_.size()) {
            if (is_space(text_[position_])) {
                if (text_[position_] == '\n')
                    ++line_;
                ++position_;
                continue;
            }
            if (text_[position_] == '/' && position_ + 1 < text_.size() &&
                text_[position_ + 1] == '/') {
                position_ += 2;
                while (position_ < text_.size() && text_[position_] != '\n')
                    ++position_;
                continue;
            }
            break;
        }
    }

    Token read_string() {
        const std::size_t token_line = line_;
        ++position_;
        const std::size_t begin = position_;
        while (position_ < text_.size() && text_[position_] != '"') {
            if (text_[position_] == '\n')
                ++line_;
            ++position_;
        }
        if (position_ >= text_.size()) {
            failed_ = true;
            return {TokenKind::string, std::string(text_.substr(begin)), token_line};
        }
        Token token{TokenKind::string, std::string(text_.substr(begin, position_ - begin)),
                    token_line};
        ++position_;
        return token;
    }

    Token read_word() {
        const std::size_t token_line = line_;
        const std::size_t begin = position_;
        while (position_ < text_.size()) {
            const char c = text_[position_];
            if (is_space(c) || c == '{' || c == '}' || c == ':' || c == ';' || c == '"')
                break;
            if (c == '/' && position_ + 1 < text_.size() && text_[position_ + 1] == '/')
                break;
            ++position_;
        }
        return {TokenKind::word, std::string(text_.substr(begin, position_ - begin)), token_line};
    }

    std::string_view text_;
    std::size_t position_ = 0;
    std::size_t line_ = 1;
    bool failed_ = false;
};

Token next_non_semicolon(Lexer& lexer) {
    Token token = lexer.next();
    while (!lexer.failed() && token.kind == TokenKind::semicolon)
        token = lexer.next();
    return token;
}

bool token_text(const Token& token, std::string& out) {
    if (token.kind != TokenKind::word && token.kind != TokenKind::string)
        return false;
    out = to_utf8(token.text);
    return true;
}

bool token_int(const Token& token, std::int32_t& out) {
    if (token.kind != TokenKind::word)
        return false;
    int parsed = 0;
    if (!parse_int(token.text, parsed))
        return false;
    out = parsed;
    return true;
}

VoidResult parse_person_fields(Lexer& lexer, SefPerson& person) {
    while (true) {
        const Token key = next_non_semicolon(lexer);
        if (lexer.failed())
            return format_error_at("unterminated SEF persons section", key.line);
        if (key.kind == TokenKind::end)
            return format_error_at("truncated SEF person block", key.line);
        if (key.kind == TokenKind::rbrace)
            return VoidResult{std::monostate{}};
        if (key.kind != TokenKind::word)
            return format_error_at("unexpected token in SEF person block", key.line);

        Token value = lexer.next();
        if (lexer.failed())
            return format_error_at("unterminated SEF persons section", key.line);
        if (value.kind == TokenKind::colon) {
            value = lexer.next();
            if (lexer.failed())
                return format_error_at("unterminated SEF persons section", key.line);
        }
        if (value.kind == TokenKind::end)
            return format_error_at("truncated SEF person field", value.line);

        if (key.text == "position") {
            if (!token_int(value, person.x))
                return format_error_at("bad SEF person position", value.line);
            const Token y = lexer.next();
            if (lexer.failed())
                return format_error_at("unterminated SEF persons section", key.line);
            if (!token_int(y, person.y))
                return format_error_at("bad SEF person position", value.line);
        } else if (key.text == "direction") {
            if (!token_text(value, person.direction))
                return format_error_at("bad SEF person direction", value.line);
        } else if (key.text == "route_type") {
            if (!token_text(value, person.route_type))
                return format_error_at("bad SEF person route_type", value.line);
        } else if (key.text == "route") {
            if (!token_text(value, person.route))
                return format_error_at("bad SEF person route", value.line);
        } else if (key.text == "radius") {
            if (!token_int(value, person.radius))
                return format_error_at("bad SEF person radius", value.line);
        } else if (key.text == "delay_min") {
            if (!token_int(value, person.delay_min))
                return format_error_at("bad SEF person delay_min", value.line);
        } else if (key.text == "delay_max") {
            if (!token_int(value, person.delay_max))
                return format_error_at("bad SEF person delay_max", value.line);
        } else if (key.text == "tribe") {
            if (!token_text(value, person.tribe))
                return format_error_at("bad SEF person tribe", value.line);
        } else if (key.text == "scr_dialog") {
            if (!token_text(value, person.dialog))
                return format_error_at("bad SEF person scr_dialog", value.line);
        } else if (key.text == "scr_inv") {
            if (!token_text(value, person.inventory))
                return format_error_at("bad SEF person scr_inv", value.line);
        } else if (value.kind == TokenKind::lbrace) {
            lexer.skip_balanced();
            if (lexer.failed())
                return format_error_at("unterminated SEF person block", value.line);
        }
    }
}

Result<std::vector<SefPerson>> parse_persons(Lexer& lexer) {
    const Token open = next_non_semicolon(lexer);
    if (lexer.failed())
        return format_error_at("unterminated SEF persons section", open.line);
    if (open.kind != TokenKind::lbrace)
        return format_error_at("SEF persons section has no opening brace", open.line);

    std::vector<SefPerson> persons;
    while (true) {
        const Token key = next_non_semicolon(lexer);
        if (lexer.failed())
            return format_error_at("unterminated SEF persons section", key.line);
        if (key.kind == TokenKind::end)
            return format_error_at("truncated SEF persons section", key.line);
        if (key.kind == TokenKind::rbrace)
            return persons;
        if (key.kind != TokenKind::word || key.text != "name")
            return format_error_at("SEF person has no name", key.line);

        Token name = lexer.next();
        if (lexer.failed())
            return format_error_at("unterminated SEF persons section", name.line);
        if (name.kind == TokenKind::colon) {
            name = lexer.next();
            if (lexer.failed())
                return format_error_at("unterminated SEF persons section", name.line);
        }

        SefPerson person;
        if (!token_text(name, person.name))
            return format_error_at("SEF person name is not a string", name.line);

        const Token block = next_non_semicolon(lexer);
        if (lexer.failed())
            return format_error_at("unterminated SEF persons section", block.line);
        if (block.kind != TokenKind::lbrace)
            return format_error_at("SEF person has no block", block.line);

        auto status = parse_person_fields(lexer, person);
        if (!status)
            return status.error();
        persons.push_back(std::move(person));
    }
}
} // namespace

Result<SefData> parse_sef(std::span<const std::uint8_t> data) {
    try {
        SefData sef;
        const std::string text(reinterpret_cast<const char*>(data.data()), data.size());
        Lexer lexer(text);
        bool found_pack = false;

        while (true) {
            const Token key = lexer.next();
            if (lexer.failed())
                return format_error_at("unterminated string in SEF", key.line);
            if (key.kind == TokenKind::end)
                break;
            if (key.kind == TokenKind::semicolon)
                continue;
            if (key.kind == TokenKind::lbrace) {
                lexer.skip_balanced();
                if (lexer.failed())
                    return format_error_at("unterminated SEF block", key.line);
                continue;
            }
            if (key.kind != TokenKind::word)
                continue;

            const Token separator = lexer.next();
            if (lexer.failed())
                return format_error_at("unterminated string in SEF", separator.line);
            if (separator.kind != TokenKind::colon) {
                if (separator.kind == TokenKind::lbrace) {
                    lexer.skip_balanced();
                    if (lexer.failed())
                        return format_error_at("unterminated SEF block", separator.line);
                }
                continue;
            }

            if (key.text == "persons") {
                auto persons = parse_persons(lexer);
                if (!persons)
                    return persons.error();
                sef.persons = std::move(persons.value());
                continue;
            }

            const Token value = lexer.next();
            if (lexer.failed())
                return format_error_at("unterminated string in SEF", value.line);
            if (value.kind == TokenKind::lbrace) {
                lexer.skip_balanced();
                if (lexer.failed())
                    return format_error_at("unterminated SEF block", value.line);
                continue;
            }
            if (value.kind == TokenKind::end)
                break;

            if (key.text == "pack") {
                if (value.kind != TokenKind::string)
                    return format_error_at("SEF pack value is not a quoted string", value.line);
                sef.pack = ascii_lower(value.text);
                found_pack = true;
            } else if (key.text == "internal_location" || key.text == "exit_to_global_map" ||
                       key.text == "exit_to_globalmap") {
                int number = 0;
                if (!parse_int(value.text, number))
                    return format_error_at("bad SEF " + std::string(key.text), value.line);
                if (key.text == "internal_location")
                    sef.internal_location = number != 0;
                else
                    sef.exit_to_global_map = number != 0;
            } else if (key.text == "weather") {
                int number = 0;
                if (!parse_int(value.text, number))
                    return format_error_at("bad SEF weather", value.line);
                sef.weather = number;
            }
        }

        if (!found_pack)
            return format_error("SEF is missing the pack field");
        return sef;
    } catch (const std::bad_alloc&) {
        return format_error("SEF allocation failed");
    }
}
} // namespace gl::assets
