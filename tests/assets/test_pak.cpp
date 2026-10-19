#include <doctest/doctest.h>
#include "gl/assets/pak.hpp"
#include <miniz.h>
#include <atomic>
#include <chrono>
#include <cstdint>
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
Bytes zlib_compress(const Bytes& in) {
    mz_ulong out_len = mz_compressBound(in.size());
    Bytes out(out_len);
    REQUIRE(mz_compress(out.data(), &out_len, in.data(), in.size()) == MZ_OK);
    out.resize(out_len);
    return out;
}
Bytes zlib_payload(const Bytes& content) {
    Bytes compressed = zlib_compress(content);
    Bytes payload;
    put_u32(payload, 1);
    put_u32(payload, static_cast<std::uint32_t>(compressed.size()));
    payload.insert(payload.end(), compressed.begin(), compressed.end());
    return payload;
}
Bytes raw_payload(const Bytes& content) {
    Bytes payload;
    put_u32(payload, 0);
    payload.insert(payload.end(), content.begin(), content.end());
    return payload;
}
struct PayloadFile {
    std::string name;
    std::uint32_t unpacked_size = 0;
    Bytes payload;
};
Bytes make_pak_from_payloads(const std::vector<PayloadFile>& files) {
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
std::filesystem::path unique_temp_path() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("gl_pak_test_" + std::to_string(stamp) + "_" +
            std::to_string(counter.fetch_add(1)) + ".pak");
}
Bytes make_pak(const std::vector<std::pair<std::string, Bytes>>& files) {
    Bytes header = {'P','A','K',' ',0,0,0,0};
    put_u32(header, static_cast<std::uint32_t>(files.size()));
    Bytes fat;
    std::vector<Bytes> payloads;
    for (const auto& [name, content] : files) {
        put_u32(fat, static_cast<std::uint32_t>(name.size()));
        fat.insert(fat.end(), name.begin(), name.end());
        Bytes compressed = zlib_compress(content);
        Bytes payload;
        put_u32(payload, 1);
        put_u32(payload, static_cast<std::uint32_t>(compressed.size()));
        payload.insert(payload.end(), compressed.begin(), compressed.end());
        put_u32(fat, static_cast<std::uint32_t>(content.size()));
        put_u32(fat, 0); // offset patched below
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
}

TEST_CASE("PAK: reads zlib entries from memory") {
    Bytes pak = make_pak({{"scripts/a.scr", {'h','e','l','l','o'}}});
    auto archive = PakArchive::open_from_memory(std::move(pak));
    REQUIRE(archive.has_value());
    REQUIRE(archive->entries().size() == 1);
    CHECK(archive->entries()[0].name == "scripts/a.scr");
    auto data = archive->read(archive->entries()[0]);
    REQUIRE(data.has_value());
    CHECK(Bytes(data->begin(), data->end()) == Bytes{'h','e','l','l','o'});
}

TEST_CASE("PAK: rejects bad magic and truncated data") {
    Bytes bad = {'N','O','P','E',0,0,0,0};
    auto r = PakArchive::open_from_memory(std::move(bad));
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code == gl::ErrorCode::format);

    Bytes pak = make_pak({{"a", {'x'}}});
    pak.resize(pak.size() - 1);
    auto t = PakArchive::open_from_memory(std::move(pak));
    // Truncation may be detected at read time, not open time.
    if (t.has_value()) {
        auto data = t->read(t->entries()[0]);
        CHECK_FALSE(data.has_value());
    }
}

TEST_CASE("PAK: opens from a file") {
    Bytes pak = make_pak({{"a", {'x','y'}}, {"b", {'z'}}});
    auto path = unique_temp_path();
    { std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(pak.data()), pak.size()); }
    auto archive = PakArchive::open_from_file(path);
    REQUIRE(archive.has_value());
    CHECK(archive->entries().size() == 2);
    auto data = archive->read(archive->entries()[1]);
    REQUIRE(data.has_value());
    CHECK(Bytes(data->begin(), data->end()) == Bytes{'z'});
    std::filesystem::remove(path);
}

TEST_CASE("PAK: reads raw entries from a file") {
    const Bytes raw = {'r','a','w',0x00,0xFF,0x10,0x20};
    Bytes pak = make_pak_from_payloads(
        {{"raw/file.dat", static_cast<std::uint32_t>(raw.size()), raw_payload(raw)}});
    auto path = unique_temp_path();
    { std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(pak.data()), pak.size()); }
    auto archive = PakArchive::open_from_file(path);
    REQUIRE(archive.has_value());
    REQUIRE(archive->entries().size() == 1);
    auto data = archive->read(archive->entries()[0]);
    REQUIRE(data.has_value());
    CHECK(Bytes(data->begin(), data->end()) == raw);
    std::filesystem::remove(path);
}

TEST_CASE("PAK: lookup is case-insensitive and slash-normalized") {
    Bytes pak = make_pak({{"Scripts\\A.SCR", {'x'}}});
    auto archive = PakArchive::open_from_memory(std::move(pak));
    REQUIRE(archive.has_value());
    CHECK(archive->find("scripts/a.scr") != nullptr);
}

TEST_CASE("PAK: reads raw (uncompressed) entries") {
    const Bytes raw = {'r','a','w',0x00,0xFF};
    Bytes pak = make_pak_from_payloads(
        {{"raw/bin.dat", static_cast<std::uint32_t>(raw.size()), raw_payload(raw)}});
    auto archive = PakArchive::open_from_memory(std::move(pak));
    REQUIRE(archive.has_value());
    const PakEntry* entry = archive->find("raw/bin.dat");
    REQUIRE(entry != nullptr);
    auto data = archive->read(*entry);
    REQUIRE(data.has_value());
    CHECK(Bytes(data->begin(), data->end()) == raw);
}

TEST_CASE("PAK: reads zero-length uncompressed entries") {
    Bytes pak = make_pak_from_payloads({{"empty", 0, zlib_payload({})}});
    auto archive = PakArchive::open_from_memory(std::move(pak));
    REQUIRE(archive.has_value());
    auto data = archive->read(archive->entries()[0]);
    REQUIRE(data.has_value());
    CHECK(data->empty());
}

TEST_CASE("PAK: rejects corrupt zlib") {
    Bytes payload;
    put_u32(payload, 1);
    put_u32(payload, 8);
    const Bytes garbage = {0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF};
    payload.insert(payload.end(), garbage.begin(), garbage.end());
    Bytes pak = make_pak_from_payloads({{"bad.bin", 8, payload}});
    auto archive = PakArchive::open_from_memory(std::move(pak));
    REQUIRE(archive.has_value());
    auto data = archive->read(archive->entries()[0]);
    REQUIRE_FALSE(data.has_value());
    CHECK(data.error().code == gl::ErrorCode::format);
}
