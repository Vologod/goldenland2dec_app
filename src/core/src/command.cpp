#include "gl/core/command.hpp"
#include <algorithm>
#include <cstddef>
#include <utility>

namespace gl::core {
namespace {
bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
}

std::vector<std::string> split_command_line(std::string_view line) {
    std::vector<std::string> tokens;
    std::string token;
    bool in_token = false;
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (in_quotes) {
            if (c == '\\' && i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\')) {
                token.push_back(line[++i]);
            } else if (c == '"') {
                in_quotes = false;
            } else {
                token.push_back(c);
            }
            continue;
        }

        if (c == '"') {
            in_quotes = true;
            in_token = true;
            continue;
        }

        if (is_space(c)) {
            if (in_token) {
                tokens.push_back(std::move(token));
                token.clear();
                in_token = false;
            }
            continue;
        }

        token.push_back(c);
        in_token = true;
    }

    if (in_token)
        tokens.push_back(std::move(token));
    return tokens;
}

void CommandRegistry::add(std::string name, std::string help, Handler handler) {
    commands_.push_back(CommandInfo{std::move(name), std::move(help), std::move(handler)});
}

gl::Result<std::string> CommandRegistry::execute(std::string_view line) const {
    const std::vector<std::string> tokens = split_command_line(line);
    if (tokens.empty())
        return std::string{};

    const std::string& name = tokens.front();
    const CommandInfo* command = nullptr;
    for (const CommandInfo& candidate : commands_) {
        if (candidate.name == name) {
            command = &candidate;
            break;
        }
    }
    if (command == nullptr) {
        return gl::Error{gl::ErrorCode::format, "unknown command: " + name};
    }
    if (!command->handler) {
        return gl::Error{gl::ErrorCode::format, "command has no handler: " + name};
    }

    std::vector<std::string_view> args;
    args.reserve(tokens.size() - 1);
    for (std::size_t i = 1; i < tokens.size(); ++i)
        args.emplace_back(tokens[i]);
    return command->handler(Args(args.data(), args.size()));
}

std::string CommandRegistry::help() const {
    std::vector<const CommandInfo*> sorted;
    sorted.reserve(commands_.size());
    for (const CommandInfo& command : commands_)
        sorted.push_back(&command);
    std::sort(sorted.begin(), sorted.end(),
              [](const CommandInfo* lhs, const CommandInfo* rhs) { return lhs->name < rhs->name; });

    std::string out;
    for (const CommandInfo* command : sorted) {
        if (!out.empty())
            out.push_back('\n');
        out += command->name;
        out += " - ";
        out += command->help;
    }
    return out;
}

std::optional<std::string> CommandRegistry::complete(std::string_view prefix) const {
    const CommandInfo* match = nullptr;
    for (const CommandInfo& command : commands_) {
        if (command.name.size() < prefix.size() ||
            command.name.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        if (match != nullptr)
            return std::nullopt;
        match = &command;
    }
    if (match == nullptr)
        return std::nullopt;
    return match->name;
}

const std::vector<CommandInfo>& CommandRegistry::commands() const { return commands_; }
}
