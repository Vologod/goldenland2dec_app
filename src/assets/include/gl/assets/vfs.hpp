#pragma once
#include "gl/assets/bytes.hpp"
#include "gl/assets/pak.hpp"
#include "gl/assets/result.hpp"
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::assets {
struct ArchiveInfo {
    std::string name;
    std::size_t entry_count = 0;
    bool is_update = false;
    int update_number = 0;
};

struct VfsEntry {
    std::string path;
    std::uint64_t size = 0;
    std::string source;
};

class GameVfs {
public:
    static Result<GameVfs> mount(const std::filesystem::path& game_dir);
    Result<Bytes> read(std::string_view virtual_path) const;
    bool exists(std::string_view virtual_path) const;
    std::vector<VfsEntry> list() const;
    const std::vector<ArchiveInfo>& archives() const { return archives_; }
    const PakArchive* archive(std::string_view file_name) const;

private:
    struct Location {
        const PakArchive* pak = nullptr;
        const PakEntry* entry = nullptr;
        std::string source;
    };
    std::optional<std::filesystem::path> loose_path(std::string_view virtual_path) const;

    std::filesystem::path game_dir_;
    std::vector<std::unique_ptr<PakArchive>> paks_;
    std::vector<ArchiveInfo> archives_;
    std::map<std::string, Location, std::less<>> index_;
};
}
