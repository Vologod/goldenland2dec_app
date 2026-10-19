#include <doctest/doctest.h>
#include "gl/assets/vfs.hpp"
#include <miniz.h>
#include <filesystem>
#include <fstream>

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
        put_u32(payload, 0);
        put_u32(payload, static_cast<std::uint32_t>(content.size()));
        payload.insert(payload.end(), compressed.begin(), compressed.end());
        put_u32(fat, static_cast<std::uint32_t>(payload.size()));
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
void write_file(const std::filesystem::path& path, const Bytes& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
}
}

TEST_CASE("VFS: loose file wins, then update N descending, then base") {
    auto dir = std::filesystem::temp_directory_path() / "gl_vfs_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "Data");
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
    auto v2 = vfs2->read("a/b.txt");
    REQUIRE(v2.has_value());
    CHECK(std::string(v2->begin(), v2->end()) == "loose");

    std::filesystem::remove_all(dir);
}
