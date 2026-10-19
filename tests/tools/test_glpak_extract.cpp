#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <miniz.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <process.h>
#endif

#ifndef GLPAK_EXE
#error "GLPAK_EXE must be defined by the build"
#endif

namespace {
using Bytes = std::vector<std::uint8_t>;
namespace fs = std::filesystem;

const fs::path g_glpak_exe = GLPAK_EXE;

void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

Bytes zlib_compress(const Bytes& in) {
    mz_ulong out_len = mz_compressBound(in.size());
    Bytes out(out_len);
    REQUIRE(mz_compress(out.data(), &out_len, in.data(), in.size()) == MZ_OK);
    out.resize(out_len);
    return out;
}

struct PayloadFile {
    std::string name;
    std::uint32_t unpacked_size = 0;
    Bytes payload;
};

Bytes make_pak(const std::vector<PayloadFile>& files) {
    Bytes header = {'P','A','K',' ',0,0,0,0};
    put_u32(header, static_cast<std::uint32_t>(files.size()));
    std::uint32_t fat_size = 0;
    for (const auto& file : files)
        fat_size += 4 + static_cast<std::uint32_t>(file.name.size()) + 4 + 4;
    std::uint32_t running = static_cast<std::uint32_t>(header.size()) + fat_size;
    Bytes fat;
    for (const auto& file : files) {
        put_u32(fat, static_cast<std::uint32_t>(file.name.size()));
        fat.insert(fat.end(), file.name.begin(), file.name.end());
        put_u32(fat, file.unpacked_size);
        put_u32(fat, running);
        running += static_cast<std::uint32_t>(file.payload.size());
    }
    Bytes pak = header;
    pak.insert(pak.end(), fat.begin(), fat.end());
    for (const auto& file : files) pak.insert(pak.end(), file.payload.begin(), file.payload.end());
    return pak;
}

Bytes zlib_payload(const Bytes& content) {
    const Bytes compressed = zlib_compress(content);
    Bytes payload;
    put_u32(payload, 1);
    put_u32(payload, static_cast<std::uint32_t>(compressed.size()));
    payload.insert(payload.end(), compressed.begin(), compressed.end());
    return payload;
}

Bytes corrupt_payload() {
    Bytes payload;
    put_u32(payload, 1);
    put_u32(payload, 8);
    const Bytes garbage = {0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF};
    payload.insert(payload.end(), garbage.begin(), garbage.end());
    return payload;
}

fs::path unique_temp_dir() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
           ("gl_glpak_cli_" + std::to_string(stamp) + "_" + std::to_string(counter.fetch_add(1)));
}

void write_file(const fs::path& path, const Bytes& data) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.is_open());
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    REQUIRE(out.good());
}

Bytes read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

#ifndef _WIN32
int process_exit_code(int status) {
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
}
#endif

int run_glpak(const std::vector<std::string>& arguments) {
#ifdef _WIN32
    std::vector<std::string> owned;
    owned.push_back(g_glpak_exe.string());
    for (const auto& arg : arguments) owned.push_back(arg);
    std::vector<const char*> argv;
    argv.reserve(owned.size() + 1);
    for (const auto& arg : owned) argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    return static_cast<int>(_spawnv(_P_WAIT, g_glpak_exe.string().c_str(), argv.data()));
#else
    std::string command = "\"" + g_glpak_exe.string() + "\"";
    for (const auto& arg : arguments) command += " \"" + arg + "\"";
    return process_exit_code(std::system(command.c_str()));
#endif
}
} // namespace

int main(int argc, char** argv) {
    doctest::Context context(argc, argv);
    return context.run();
}

TEST_CASE("glpak extract batch continues after a corrupt PAK entry") {
    const fs::path root = unique_temp_dir();
    fs::remove_all(root);
    const Bytes good = {'o','k','\n'};
    const Bytes pak = make_pak({
        {"batch/bad.bin", 8, corrupt_payload()},
        {"batch/good.txt", static_cast<std::uint32_t>(good.size()), zlib_payload(good)},
    });
    write_file(root / "game" / "Data" / "corrupt.pak", pak);

    const std::vector<std::pair<std::vector<std::string>, std::string>> selectors = {
        {{"--all"}, "all"},
        {{"--archive", "corrupt.pak"}, "archive"},
        {{"--prefix", "batch"}, "prefix"},
    };
    for (const auto& [selector, label] : selectors) {
        CAPTURE(label);
        const fs::path out = root / ("out_" + label);
        std::vector<std::string> arguments = {"extract", (root / "game").string()};
        arguments.insert(arguments.end(), selector.begin(), selector.end());
        arguments.push_back("--out");
        arguments.push_back(out.string());
        const int code = run_glpak(arguments);
        CHECK(code == 3);
        CHECK(read_file(out / "batch" / "good.txt") == good);
    }

    fs::remove_all(root);
}
