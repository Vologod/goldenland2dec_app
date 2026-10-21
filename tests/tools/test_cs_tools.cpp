#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#endif

#ifndef GLPAK_EXE
#error "GLPAK_EXE must be defined by the build"
#endif
#ifndef GLSCRIPT_EXE
#error "GLSCRIPT_EXE must be defined by the build"
#endif

namespace {
namespace fs = std::filesystem;

const fs::path g_glpak_exe = GLPAK_EXE;
const fs::path g_glscript_exe = GLSCRIPT_EXE;
constexpr std::string_view kDialogScript = "scripts/dialogs/demon.d1.age.cs";

const char* game_dir_env() {
    const char* value = std::getenv("GL_GAME_DIR");
    return value != nullptr && value[0] != '\0' ? value : nullptr;
}

bool game_dir_available() { return game_dir_env() != nullptr; }

fs::path unique_temp_dir() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
           ("gl_cs_tools_" + std::to_string(stamp) + "_" + std::to_string(counter.fetch_add(1)));
}

struct TempDir {
    TempDir() : path(unique_temp_dir()) {
        fs::remove_all(path);
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code error;
        fs::remove_all(path, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    fs::path path;
};

std::string quote_argument(const std::string& arg) {
    if (arg.find_first_of(" \t\"") == std::string::npos) return arg;
    std::string quoted = "\"";
    for (const char c : arg) {
        if (c == '"') quoted += "\\\"";
        else quoted += c;
    }
    quoted += '"';
    return quoted;
}

#ifndef _WIN32
std::string command_line(const fs::path& executable, const std::vector<std::string>& arguments) {
    std::string command = quote_argument(executable.string());
    for (const auto& argument : arguments) command += " " + quote_argument(argument);
    return command;
}
#endif

int run_process(const fs::path& executable, const std::vector<std::string>& arguments) {
#ifdef _WIN32
    std::vector<std::string> owned;
    owned.push_back(quote_argument(executable.string()));
    for (const auto& argument : arguments) owned.push_back(quote_argument(argument));
    std::vector<const char*> argv;
    argv.reserve(owned.size() + 1);
    for (const auto& item : owned) argv.push_back(item.c_str());
    argv.push_back(nullptr);
    return static_cast<int>(_spawnv(_P_WAIT, executable.string().c_str(), argv.data()));
#else
    const int status = std::system(command_line(executable, arguments).c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif
}

std::string read_file(const fs::path& path);

int run_capture(const fs::path& executable, const std::vector<std::string>& arguments,
                std::string& output) {
#ifdef _WIN32
    const fs::path root = unique_temp_dir();
    const fs::path capture = root / "stdout.txt";
    std::error_code error;
    fs::create_directories(root, error);
    REQUIRE_FALSE(error);

    const int descriptor =
        _open(capture.string().c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
              _S_IREAD | _S_IWRITE);
    REQUIRE(descriptor != -1);

    std::fflush(stdout);
    const int saved = _dup(1);
    REQUIRE(saved != -1);
    REQUIRE(_dup2(descriptor, 1) == 0);
    _close(descriptor);

    const int code = run_process(executable, arguments);

    std::fflush(stdout);
    _dup2(saved, 1);
    _close(saved);

    output = read_file(capture);
    fs::remove(capture, error);
    fs::remove(root, error);
    return code;
#else
    const std::string command = command_line(executable, arguments);
    FILE* pipe = popen(command.c_str(), "r");
    REQUIRE(pipe != nullptr);

    char buffer[4096];
    std::size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), pipe)) > 0)
        output.append(buffer, count);

    const int status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void write_file(const fs::path& path, const std::vector<std::uint8_t>& data) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    REQUIRE_FALSE(error);
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.is_open());
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    REQUIRE(out.good());
}

bool has_status(const std::string& output, std::string_view status) {
    return output.find("status: " + std::string(status)) != std::string::npos;
}

std::size_t metric_count(const std::string& output, std::string_view name, bool last) {
    const std::string prefix = std::string(name) + ": ";
    const std::size_t start = last ? output.rfind(prefix) : output.find(prefix);
    REQUIRE(start != std::string::npos);
    return static_cast<std::size_t>(std::stoul(output.substr(start + prefix.size())));
}

std::size_t answer_count(const std::string& output) {
    return metric_count(output, "answers", true);
}

bool has_cyrillic_utf8(std::string_view text) {
    bool cyrillic = false;
    std::size_t index = 0;
    while (index < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        std::size_t length = 0;
        char32_t codepoint = 0;
        if (lead < 0x80) {
            codepoint = lead;
            length = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            codepoint = lead & 0x1F;
            length = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            codepoint = lead & 0x0F;
            length = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            codepoint = lead & 0x07;
            length = 4;
        } else {
            return false;
        }
        if (index + length > text.size()) return false;
        for (std::size_t offset = 1; offset < length; ++offset) {
            const unsigned char continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0) != 0x80) return false;
            codepoint = (codepoint << 6) | (continuation & 0x3F);
        }
        if (codepoint >= 0x0400 && codepoint <= 0x04FF) cyrillic = true;
        index += length;
    }
    return cyrillic;
}

std::string phrase_line(const std::string& output, std::string_view id) {
    const std::string prefix = std::string(id) + "\t";
    std::size_t search = 0;
    while ((search = output.find(prefix, search)) != std::string::npos) {
        if (search == 0 || output[search - 1] == '\n') {
            const std::size_t end = output.find('\n', search);
            return end == std::string::npos ? output.substr(search)
                                            : output.substr(search, end - search);
        }
        search += prefix.size();
    }
    return {};
}
} // namespace

int main(int argc, char** argv) {
    doctest::Context context(argc, argv);
    const int result = context.run();
    if (game_dir_env() == nullptr && result == 0) return 77;
    return result;
}

TEST_CASE("glpak disasm-cs dumps the demon dialog" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = game_dir_env();
    REQUIRE(game_dir != nullptr);

    TempDir temp;
    const fs::path dump = temp.path / "demon.txt";
    const int code = run_process(g_glpak_exe, {"disasm-cs", game_dir, "--file",
                                               std::string(kDialogScript), "--out", dump.string()});
    CAPTURE(code);
    CHECK(code == 0);
    REQUIRE(fs::exists(dump));

    const std::string text = read_file(dump);
    CHECK_FALSE(text.empty());
    CHECK(text.find("func") != std::string::npos);
    CHECK(text.find("jmp") != std::string::npos);

    std::string stdout_text;
    const int stdout_code =
        run_capture(g_glpak_exe, {"disasm-cs", game_dir, "--file", std::string(kDialogScript)},
                    stdout_text);
    CAPTURE(stdout_code);
    CHECK(stdout_code == 0);
    CHECK_FALSE(stdout_text.empty());
    CHECK(stdout_text.find("func") != std::string::npos);
}

TEST_CASE("glscript runs the demon dialog to wait_user" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = game_dir_env();
    REQUIRE(game_dir != nullptr);

    std::string output;
    const int code = run_capture(g_glscript_exe, {"--game-dir", game_dir, "--file",
                                                  std::string(kDialogScript), "--run"},
                                 output);
    CAPTURE(code);
    CAPTURE(output);
    CHECK(code == 0);
    CHECK(has_status(output, "wait_user"));
    CHECK(answer_count(output) > 0);
}

TEST_CASE("glscript --sdb prints dialog phrase texts" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = game_dir_env();
    REQUIRE(game_dir != nullptr);

    std::string output;
    const int code = run_capture(g_glscript_exe,
                                 {"--game-dir", game_dir, "--file", std::string(kDialogScript),
                                  "--run", "--sdb", "sdb/dialogs/dialogsphrases.sdb"},
                                 output);
    CAPTURE(code);
    CAPTURE(output);
    CHECK(code == 0);
    CHECK(has_status(output, "wait_user"));

    const std::string line = phrase_line(output, "2684");
    CAPTURE(line);
    CHECK_FALSE(line.empty());
    CHECK(has_cyrillic_utf8(line));
    CHECK(line.find("<phrase 2684>") == std::string::npos);
}

TEST_CASE("glscript accepts the first dialog answer" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = game_dir_env();
    REQUIRE(game_dir != nullptr);

    std::string output;
    const int code = run_capture(g_glscript_exe, {"--game-dir", game_dir, "--file",
                                                  std::string(kDialogScript), "--run",
                                                  "--answer", "0"},
                                 output);
    CAPTURE(code);
    CAPTURE(output);
    CHECK(code == 0);
    CHECK((has_status(output, "wait_user") || has_status(output, "end")));
    CHECK(metric_count(output, "steps", true) > metric_count(output, "steps", false));
}

TEST_CASE("glscript rejects --answer without --run" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = game_dir_env();
    REQUIRE(game_dir != nullptr);

    const int code = run_process(g_glscript_exe, {"--game-dir", game_dir, "--file",
                                                  std::string(kDialogScript), "--answer", "0"});
    CAPTURE(code);
    CHECK(code == 1);
}

TEST_CASE("glscript rejects an out-of-range dialog answer" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = game_dir_env();
    REQUIRE(game_dir != nullptr);

    const int code = run_process(g_glscript_exe, {"--game-dir", game_dir, "--file",
                                                  std::string(kDialogScript), "--run",
                                                  "--answer", "99"});
    CAPTURE(code);
    CHECK(code == 1);
}

TEST_CASE("glscript reports a missing script as an I/O error" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = game_dir_env();
    REQUIRE(game_dir != nullptr);

    const int code = run_process(g_glscript_exe, {"--game-dir", game_dir, "--file",
                                                  "scripts/dialogs/does_not_exist.cs",
                                                  "--run"});
    CAPTURE(code);
    CHECK(code == 2);
}

TEST_CASE("glpak disasm-cs escapes quotes and control characters" *
          doctest::skip(!game_dir_available())) {
    TempDir temp;

    std::vector<std::uint8_t> body;
    const auto put_u32 = [](std::vector<std::uint8_t>& out, std::uint32_t value) {
        for (int i = 0; i < 4; ++i)
            out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    };
    const auto put_i32 = [&put_u32](std::vector<std::uint8_t>& out, std::int32_t value) {
        put_u32(out, static_cast<std::uint32_t>(value));
    };

    put_i32(body, 22);
    const std::string text = "a\"b\nc";
    body.insert(body.end(), text.begin(), text.end());
    body.push_back(0);
    put_i32(body, 49);
    put_i32(body, 0);
    put_i32(body, -1);

    std::vector<std::uint8_t> script;
    put_u32(script, static_cast<std::uint32_t>(body.size() + 4));
    script.insert(script.end(), body.begin(), body.end());
    write_file(temp.path / "scripts" / "escape.cs", script);

    const fs::path dump = temp.path / "escape.txt";
    const int code = run_process(g_glpak_exe, {"disasm-cs", temp.path.string(), "--file",
                                               "scripts/escape.cs", "--out", dump.string()});
    CAPTURE(code);
    CHECK(code == 0);

    const std::string disassembly = read_file(dump);
    CHECK(disassembly.find("a\\\"b\\nc") != std::string::npos);
    std::size_t newlines = 0;
    for (const char c : disassembly) {
        if (c == '\n') ++newlines;
    }
    CHECK(newlines == 2);
}

TEST_CASE("glscript reports a VM execution failure" *
          doctest::skip(!game_dir_available())) {
    TempDir temp;

    std::vector<std::uint8_t> script;
    const auto put_u32 = [&script](std::uint32_t value) {
        for (int i = 0; i < 4; ++i)
            script.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    };
    put_u32(16);
    put_u32(49);
    put_u32(9);
    put_u32(9);
    write_file(temp.path / "scripts" / "fail.cs", script);
    write_file(temp.path / "scripts" / "empty.scr", {});

    const int code =
        run_process(g_glscript_exe, {"--game-dir", temp.path.string(), "--globals",
                                     "scripts/empty.scr", "--file", "scripts/fail.cs", "--run"});
    CAPTURE(code);
    CHECK(code == 3);
}
