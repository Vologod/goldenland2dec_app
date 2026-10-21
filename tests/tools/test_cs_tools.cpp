#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
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

bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

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

bool has_status(const std::string& output, std::string_view status) {
    return output.find("status: " + std::string(status)) != std::string::npos;
}

std::size_t answer_count(const std::string& output) {
    const std::size_t start = output.rfind("answers: ");
    REQUIRE(start != std::string::npos);
    return static_cast<std::size_t>(std::stoul(output.substr(start + 9)));
}
} // namespace

int main(int argc, char** argv) {
    doctest::Context context(argc, argv);
    const int result = context.run();
    if (std::getenv("GL_GAME_DIR") == nullptr) return 77;
    return result;
}

TEST_CASE("glpak disasm-cs dumps the demon dialog" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
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
}

TEST_CASE("glscript runs the demon dialog to wait_user" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
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

TEST_CASE("glscript accepts the first dialog answer" *
          doctest::skip(!game_dir_available())) {
    const char* game_dir = std::getenv("GL_GAME_DIR");
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
}
