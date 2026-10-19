#include <doctest/doctest.h>
#include "gl/assets/vfs.hpp"
#include <miniz.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using namespace gl::assets;

namespace {
void put_u32(Bytes& b, std::uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
Bytes make_pak(const std::vector<std::pair<std::string, Bytes>>& files) {
    Bytes header = {'P','A','K',' ',0,0,0,0};
    put_u32(header, static_cast<std::uint32_t>(files.size()));
    Bytes fat;
    std::vector<Bytes> payloads;
    for (const auto& [name, content] : files) {
        put_u32(fat, static_cast<std::uint32_t>(name.size()));
        fat.insert(fat.end(), name.begin(), name.end());
        mz_ulong bound = mz_compressBound(content.size());
        Bytes compressed(bound);
        mz_compress(compressed.data(), &bound, content.data(), content.size());
        compressed.resize(bound);
        Bytes payload;
        put_u32(payload, 1);
        put_u32(payload, static_cast<std::uint32_t>(compressed.size()));
        payload.insert(payload.end(), compressed.begin(), compressed.end());
        put_u32(fat, static_cast<std::uint32_t>(content.size()));
        put_u32(fat, 0);
        payloads.push_back(std::move(payload));
    }
    std::uint32_t running = static_cast<std::uint32_t>(header.size() + fat.size());
    std::size_t pos = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        pos += 4 + files[i].first.size() + 4;
        fat[pos + 0] = running & 0xFF;
        fat[pos + 1] = (running >> 8) & 0xFF;
        fat[pos + 2] = (running >> 16) & 0xFF;
        fat[pos + 3] = (running >> 24) & 0xFF;
        pos += 4;
        running += static_cast<std::uint32_t>(payloads[i].size());
    }
    Bytes pak = header;
    pak.insert(pak.end(), fat.begin(), fat.end());
    for (const auto& p : payloads) pak.insert(pak.end(), p.begin(), p.end());
    return pak;
}
std::filesystem::path unique_temp_dir() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("gl_vfs_test_" + std::to_string(stamp) + "_" + std::to_string(counter.fetch_add(1)));
}
void write_file(const std::filesystem::path& path, const Bytes& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.is_open());
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    REQUIRE(out.good());
}
const ArchiveInfo* find_archive(const std::vector<ArchiveInfo>& archives, std::string_view name) {
    for (const auto& info : archives) if (info.name == name) return &info;
    return nullptr;
}
}

TEST_CASE("VFS: loose file wins, then update N descending, then base") {
    auto dir = unique_temp_dir();
    std::filesystem::remove_all(dir);

    write_file(dir / "Data" / "world.pak", make_pak({{"a/b.txt", {'b','a','s','e'}}}));
    write_file(dir / "Data" / "world.update.1.pak", make_pak({{"a/b.txt", {'u','1'}}}));
    write_file(dir / "Data" / "world.update.2.pak", make_pak({{"a/b.txt", {'u','2'}}}));

    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto v = vfs->read("A\\B.TXT");
    REQUIRE(v.has_value());
    CHECK(std::string(v->begin(), v->end()) == "u2");

    write_file(dir / "a" / "b.txt", Bytes{'l','o','o','s','e'});
    auto vfs2 = GameVfs::mount(dir);
    REQUIRE(vfs2.has_value());
    auto v2 = vfs2->read("A\\B.TXT");
    REQUIRE(v2.has_value());
    CHECK(std::string(v2->begin(), v2->end()) == "loose");
    auto v3 = vfs2->read("a/b.txt");
    REQUIRE(v3.has_value());
    CHECK(std::string(v3->begin(), v3->end()) == "loose");

    std::filesystem::remove_all(dir);
}

TEST_CASE("VFS: loose files under Data and root precedence") {
    auto dir = unique_temp_dir();
    std::filesystem::remove_all(dir);

    write_file(dir / "Data" / "c" / "d.txt", Bytes{'d','a','t','a'});
    write_file(dir / "x" / "y.txt", Bytes{'r','o','o','t'});
    write_file(dir / "Data" / "x" / "y.txt", Bytes{'d','a','t','a'});

    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto data = vfs->read("C\\D.TXT");
    REQUIRE(data.has_value());
    CHECK(std::string(data->begin(), data->end()) == "data");
    CHECK(vfs->exists("C/D.TXT"));

    auto root = vfs->read("X\\Y.TXT");
    REQUIRE(root.has_value());
    CHECK(std::string(root->begin(), root->end()) == "root");

    std::filesystem::remove_all(dir);
}

TEST_CASE("VFS: exists and list cover merged archive view only") {
    auto dir = unique_temp_dir();
    std::filesystem::remove_all(dir);

    write_file(dir / "Data" / "world.pak", make_pak({{"a/b.txt", {'b','a','s','e'}}}));
    write_file(dir / "loose.txt", Bytes{'l','o','o','s','e'});

    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    CHECK(vfs->exists("A\\B.TXT"));
    CHECK(vfs->exists("loose.txt"));
    CHECK_FALSE(vfs->exists("missing.txt"));
    CHECK_FALSE(vfs->exists("../loose.txt"));

    const auto entries = vfs->list();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].path == "a/b.txt");
    CHECK(entries[0].uncompressed_size > 0);
    CHECK(entries[0].source == "world.pak");

    std::filesystem::remove_all(dir);
}

TEST_CASE("VFS: archive metadata, case-insensitive .PAK discovery, exact lookup") {
    auto dir = unique_temp_dir();
    std::filesystem::remove_all(dir);

    write_file(dir / "Data" / "world.pak", make_pak({{"a/b.txt", {'b','a','s','e'}}}));
    write_file(dir / "Data" / "world.update.1.pak", make_pak({{"a/b.txt", {'u','1'}}}));
    write_file(dir / "Data" / "world.update.2.pak",
               make_pak({{"a/b.txt", {'u','2'}}, {"c/d.txt", {'u','2'}}}));
    write_file(dir / "Data" / "EXTRA.PAK", make_pak({{"e/f.txt", {'e','x','t','r','a'}}}));
    write_file(dir / "world.update.pak", make_pak({{"g/h.txt", {'n','o','n','u','m'}}}));
    write_file(dir / "world.update.99999999999999999999.pak",
               make_pak({{"i/j.txt", {'o','v','e','r'}}}));

    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    REQUIRE(vfs->archives().size() == 6);

    const auto* base = find_archive(vfs->archives(), "world.pak");
    REQUIRE(base != nullptr);
    CHECK(base->entry_count == 1);
    CHECK_FALSE(base->is_update);

    const auto* u1 = find_archive(vfs->archives(), "world.update.1.pak");
    REQUIRE(u1 != nullptr);
    CHECK(u1->entry_count == 1);
    CHECK(u1->is_update);
    CHECK(u1->update_number == 1);

    const auto* u2 = find_archive(vfs->archives(), "world.update.2.pak");
    REQUIRE(u2 != nullptr);
    CHECK(u2->entry_count == 2);
    CHECK(u2->is_update);
    CHECK(u2->update_number == 2);

    const auto* extra = find_archive(vfs->archives(), "EXTRA.PAK");
    REQUIRE(extra != nullptr);
    CHECK(extra->entry_count == 1);
    CHECK_FALSE(extra->is_update);

    const auto* no_digits = find_archive(vfs->archives(), "world.update.pak");
    REQUIRE(no_digits != nullptr);
    CHECK_FALSE(no_digits->is_update);

    const auto* overflow = find_archive(vfs->archives(), "world.update.99999999999999999999.pak");
    REQUIRE(overflow != nullptr);
    CHECK_FALSE(overflow->is_update);

    CHECK(vfs->archive("world.pak") != nullptr);
    CHECK(vfs->archive("EXTRA.PAK") != nullptr);
    CHECK(vfs->archive("missing.pak") == nullptr);
    CHECK(vfs->archive("WORLD.PAK") == nullptr);

    std::filesystem::remove_all(dir);
}

TEST_CASE("VFS: equal-priority base archives are deterministic by file name") {
    auto dir = unique_temp_dir();
    std::filesystem::remove_all(dir);

    write_file(dir / "alpha.pak", make_pak({{"tie.txt", {'a','l','p','h','a'}}}));
    write_file(dir / "beta.pak", make_pak({{"tie.txt", {'b','e','t','a'}}}));

    auto vfs = GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto v = vfs->read("tie.txt");
    REQUIRE(v.has_value());
    CHECK(std::string(v->begin(), v->end()) == "beta");

    std::filesystem::remove_all(dir);
}
