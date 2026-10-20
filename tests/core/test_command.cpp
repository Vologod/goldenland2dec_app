#include "gl/core/command.hpp"
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {
std::string join_args(gl::core::Args args) {
    std::string out;
    for (const std::string_view arg : args) {
        if (!out.empty())
            out.push_back('|');
        out += std::string(arg);
    }
    return out;
}
}

TEST_CASE("split_command_line: whitespace separates tokens") {
    const std::vector<std::string> args = gl::core::split_command_line("  alpha   beta\tgamma  ");
    REQUIRE(args.size() == 3);
    CHECK(args[0] == "alpha");
    CHECK(args[1] == "beta");
    CHECK(args[2] == "gamma");

    CHECK(gl::core::split_command_line("").empty());
    CHECK(gl::core::split_command_line("   \t \r\n ").empty());
}

TEST_CASE("split_command_line: quotes group spaces") {
    const std::vector<std::string> args = gl::core::split_command_line("say \"hello world\" tail");
    REQUIRE(args.size() == 3);
    CHECK(args[0] == "say");
    CHECK(args[1] == "hello world");
    CHECK(args[2] == "tail");

    const std::vector<std::string> joined = gl::core::split_command_line("pre\"mid dle\"post");
    REQUIRE(joined.size() == 1);
    CHECK(joined[0] == "premid dlepost");
}

TEST_CASE("split_command_line: empty quoted token is preserved") {
    const std::vector<std::string> args = gl::core::split_command_line("a \"\" b");
    REQUIRE(args.size() == 3);
    CHECK(args[0] == "a");
    CHECK(args[1].empty());
    CHECK(args[2] == "b");
}

TEST_CASE("split_command_line: backslash escapes inside quotes") {
    const std::vector<std::string> args =
        gl::core::split_command_line("cmd \"a \\\"quote\\\" and \\\\ slash\"");
    REQUIRE(args.size() == 2);
    CHECK(args[0] == "cmd");
    CHECK(args[1] == "a \"quote\" and \\ slash");
}

TEST_CASE("split_command_line: unterminated quote keeps the rest") {
    const std::vector<std::string> args = gl::core::split_command_line("cmd \"open ended");
    REQUIRE(args.size() == 2);
    CHECK(args[0] == "cmd");
    CHECK(args[1] == "open ended");
}

TEST_CASE("CommandRegistry: execute passes remaining args to the handler") {
    gl::core::CommandRegistry registry;
    registry.add("echo", "Echo args",
                 [](gl::core::Args args) { return gl::Result<std::string>(join_args(args)); });

    auto no_args = registry.execute("echo");
    REQUIRE(no_args.has_value());
    CHECK(no_args.value().empty());

    auto plain = registry.execute("echo hello world");
    REQUIRE(plain.has_value());
    CHECK(plain.value() == "hello|world");

    auto quoted = registry.execute("echo \"a b\" c");
    REQUIRE(quoted.has_value());
    CHECK(quoted.value() == "a b|c");
}

TEST_CASE("CommandRegistry: empty line is a successful no-op") {
    gl::core::CommandRegistry registry;
    registry.add("echo", "Echo args", [](gl::core::Args args) { return join_args(args); });

    auto empty = registry.execute("");
    REQUIRE(empty.has_value());
    CHECK(empty.value().empty());

    auto spaces = registry.execute("   ");
    REQUIRE(spaces.has_value());
    CHECK(spaces.value().empty());
}

TEST_CASE("CommandRegistry: unknown command is a format error") {
    gl::core::CommandRegistry registry;
    registry.add("echo", "Echo args", [](gl::core::Args args) { return join_args(args); });

    auto unknown = registry.execute("nope arg");
    REQUIRE_FALSE(unknown.has_value());
    CHECK(unknown.error().code == gl::ErrorCode::format);
    CHECK(unknown.error().message == "unknown command: nope");

    auto case_sensitive = registry.execute("ECHO");
    REQUIRE_FALSE(case_sensitive.has_value());
    CHECK(case_sensitive.error().code == gl::ErrorCode::format);
    CHECK(case_sensitive.error().message == "unknown command: ECHO");
}

TEST_CASE("CommandRegistry: handler errors are propagated") {
    gl::core::CommandRegistry registry;
    registry.add("fail", "Always fails", [](gl::core::Args) {
        return gl::Result<std::string>(gl::Error{gl::ErrorCode::argument, "bad argument"});
    });

    auto result = registry.execute("fail one");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::argument);
    CHECK(result.error().message == "bad argument");
}

TEST_CASE("CommandRegistry: help is sorted by name") {
    gl::core::CommandRegistry registry;
    registry.add("zeta", "last command", [](gl::core::Args) { return std::string{}; });
    registry.add("alpha", "first command", [](gl::core::Args) { return std::string{}; });
    registry.add("middle", "second command", [](gl::core::Args) { return std::string{}; });

    CHECK(registry.help() == "alpha - first command\nmiddle - second command\nzeta - last command");
}

TEST_CASE("CommandRegistry: completion handles unique, ambiguous and missing") {
    gl::core::CommandRegistry registry;
    registry.add("help", "show help", [](gl::core::Args) { return std::string{}; });
    registry.add("hello", "greet", [](gl::core::Args) { return std::string{}; });
    registry.add("exit", "leave", [](gl::core::Args) { return std::string{}; });

    CHECK(registry.complete("help") == std::optional<std::string>("help"));
    CHECK(registry.complete("hell") == std::optional<std::string>("hello"));
    CHECK(registry.complete("ex") == std::optional<std::string>("exit"));
    CHECK_FALSE(registry.complete("hel").has_value());
    CHECK_FALSE(registry.complete("he").has_value());
    CHECK_FALSE(registry.complete("x").has_value());
    CHECK_FALSE(registry.complete("").has_value());
}

TEST_CASE("CommandRegistry: completion with a single command") {
    gl::core::CommandRegistry registry;
    registry.add("only", "the only command", [](gl::core::Args) { return std::string{}; });

    CHECK(registry.complete("") == std::optional<std::string>("only"));
    CHECK(registry.complete("o") == std::optional<std::string>("only"));
    CHECK_FALSE(registry.complete("z").has_value());
}

TEST_CASE("CommandRegistry: commands keeps registration order") {
    gl::core::CommandRegistry registry;
    registry.add("zeta", "last", [](gl::core::Args) { return std::string{}; });
    registry.add("alpha", "first", [](gl::core::Args) { return std::string{}; });

    REQUIRE(registry.commands().size() == 2);
    CHECK(registry.commands()[0].name == "zeta");
    CHECK(registry.commands()[1].name == "alpha");
}
