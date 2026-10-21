#include "gl/assets/cs.hpp"
#include "gl/assets/result.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/host.hpp"
#include "gl/script/script_vm.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

enum ExitCode : int { kOk = 0, kArgument = 1, kIo = 2, kFormat = 3 };

constexpr std::string_view kDefaultGlobals = "scripts/dialogs_special/zlato_vars.scr";

int fail(const std::string& message, int code) {
    std::cerr << "glscript: " << message << '\n';
    return code;
}

int fail(const gl::Error& error) {
    int code = kFormat;
    switch (error.code) {
        case gl::ErrorCode::io: code = kIo; break;
        case gl::ErrorCode::format: code = kFormat; break;
        case gl::ErrorCode::argument: code = kArgument; break;
        case gl::ErrorCode::unsupported: code = kFormat; break;
    }
    return fail(error.message, code);
}

void print_usage() {
    std::cerr << "usage:\n"
                 "  glscript --game-dir <dir> [--globals <path>] --file <path> [--run]"
                 " [--answer N]\n";
}

bool option_value(const std::vector<std::string>& args, std::size_t& index,
                  const std::string& name, std::string& value, std::string& error) {
    if (index + 1 >= args.size() || args[index + 1].starts_with("--")) {
        error = "missing value for " + name;
        return false;
    }
    value = args[++index];
    if (value.empty()) {
        error = "empty value for " + name;
        return false;
    }
    return true;
}

bool set_once(std::optional<std::string>& slot, const std::string& name, std::string value,
              std::string& error) {
    if (slot) {
        error = "duplicate option " + name;
        return false;
    }
    slot = std::move(value);
    return true;
}

bool parse_answer(std::string_view text, std::size_t& value, std::string& error) {
    unsigned long long parsed = 0;
    const auto [end, code] =
        std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (code != std::errc{} || end != text.data() + text.size()) {
        error = "invalid --answer value: " + std::string(text);
        return false;
    }
    if (parsed > std::numeric_limits<std::size_t>::max()) {
        error = "--answer value is too large: " + std::string(text);
        return false;
    }
    value = static_cast<std::size_t>(parsed);
    return true;
}

struct Options {
    std::optional<std::string> game_dir;
    std::optional<std::string> globals;
    std::optional<std::string> file;
    bool run = false;
    std::vector<std::size_t> answers;
};

bool parse_options(const std::vector<std::string>& args, Options& options, std::string& error) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--run") {
            if (options.run) {
                error = "duplicate option --run";
                return false;
            }
            options.run = true;
            continue;
        }
        if (arg == "--game-dir" || arg == "--globals" || arg == "--file" ||
            arg == "--answer") {
            std::string value;
            if (!option_value(args, i, arg, value, error)) return false;
            if (arg == "--game-dir") {
                if (!set_once(options.game_dir, arg, std::move(value), error)) return false;
            } else if (arg == "--globals") {
                if (!set_once(options.globals, arg, std::move(value), error)) return false;
            } else if (arg == "--file") {
                if (!set_once(options.file, arg, std::move(value), error)) return false;
            } else {
                std::size_t answer = 0;
                if (!parse_answer(value, answer, error)) return false;
                options.answers.push_back(answer);
            }
            continue;
        }
        error = "unknown option: " + arg;
        return false;
    }
    if (!options.game_dir) {
        error = "missing --game-dir";
        return false;
    }
    if (!options.file) {
        error = "missing --file";
        return false;
    }
    if (!options.run && !options.answers.empty()) {
        error = "--answer requires --run";
        return false;
    }
    return true;
}

const char* status_name(gl::script::VmStatus status) {
    using gl::script::VmStatus;
    switch (status) {
    case VmStatus::start: return "start";
    case VmStatus::restart: return "restart";
    case VmStatus::continue_: return "continue";
    case VmStatus::wait_user: return "wait_user";
    case VmStatus::end: return "end";
    case VmStatus::infinity: return "infinity";
    }
    return "unknown";
}

void print_state(const gl::script::ScriptVm& vm) {
    std::cout << "status: " << status_name(vm.status()) << '\n';
    std::cout << "steps: " << vm.steps() << '\n';
    std::cout << "dialogs:";
    for (const std::uint32_t id : vm.dialogs_data()) std::cout << ' ' << id;
    std::cout << '\n';
    std::cout << "answers: " << vm.answers_count() << '\n';
}

void run_to_stop(gl::script::ScriptVm& vm) {
    while (vm.step()) {
    }
}

int run(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) args.emplace_back(argv[i]);
    if (args.size() < 2) {
        print_usage();
        return kArgument;
    }
    if (args.size() == 2 && (args[1] == "--help" || args[1] == "-h")) {
        print_usage();
        return kOk;
    }

    Options options;
    std::string error;
    if (!parse_options(args, options, error)) {
        print_usage();
        return fail(error, kArgument);
    }

    auto mounted = gl::assets::GameVfs::mount(fs::path(*options.game_dir));
    if (!mounted) return fail(mounted.error());

    const std::string globals_path =
        options.globals ? *options.globals : std::string(kDefaultGlobals);

    auto script_data = mounted.value().read(*options.file);
    if (!script_data) return fail(script_data.error());
    auto nodes = gl::assets::parse_cs(script_data.value());
    if (!nodes) return fail(nodes.error());

    auto globals_data = mounted.value().read(globals_path);
    if (!globals_data) return fail(globals_data.error());
    auto globals = gl::script::parse_globals(globals_data.value());
    if (!globals) return fail(globals.error());

    gl::script::DefaultScriptHost host;
    gl::script::ScriptVm vm(std::move(nodes.value()), host);
    vm.set_globals(globals.value());

    if (!options.run) {
        print_state(vm);
        return kOk;
    }

    vm.restart(true);
    run_to_stop(vm);
    print_state(vm);
    if (vm.failed() || vm.status() == gl::script::VmStatus::infinity) return kFormat;

    for (const std::size_t answer : options.answers) {
        if (!vm.user_input(answer)) {
            print_state(vm);
            return kArgument;
        }
        run_to_stop(vm);
        print_state(vm);
        if (vm.failed() || vm.status() == gl::script::VmStatus::infinity) return kFormat;
    }

    return kOk;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        return fail(std::string("unexpected error: ") + error.what(), kIo);
    } catch (...) {
        return fail("unexpected error", kIo);
    }
}
