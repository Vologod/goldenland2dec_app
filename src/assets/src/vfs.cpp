#include "gl/assets/vfs.hpp"
#include "gl/assets/cp1251.hpp"
#include <algorithm>
#include <fstream>
#include <utility>

namespace gl::assets {
namespace {
int update_number(const std::string& filename) {
    const std::string marker = ".update.";
    auto pos = filename.rfind(marker);
    if (pos == std::string::npos) return -1;
    auto end = filename.find(".pak", pos);
    if (end == std::string::npos) return -1;
    int value = 0;
    for (std::size_t i = pos + marker.size(); i < end; ++i) {
        if (filename[i] < '0' || filename[i] > '9') return -1;
        value = value * 10 + (filename[i] - '0');
    }
    return value;
}
}

std::optional<std::filesystem::path> GameVfs::loose_path(std::string_view vp) const {
    if (!is_safe_relative_path(vp)) return std::nullopt;
    std::string native(vp);
    for (char& c : native) if (c == '/') c = std::filesystem::path::preferred_separator;
    const auto direct = game_dir_ / native;
    if (std::filesystem::is_regular_file(direct)) return direct;
    const auto in_data = game_dir_ / "Data" / native;
    if (std::filesystem::is_regular_file(in_data)) return in_data;
    return std::nullopt;
}

Result<GameVfs> GameVfs::mount(const std::filesystem::path& game_dir) {
    if (!std::filesystem::is_directory(game_dir))
        return Error{ErrorCode::io, "game directory not found: " + game_dir.string()};
    GameVfs vfs;
    vfs.game_dir_ = game_dir;
    std::vector<std::filesystem::path> pak_paths;
    std::error_code ec;
    for (const auto& dir : {game_dir, game_dir / "Data"}) {
        if (!std::filesystem::is_directory(dir, ec)) continue;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pak")
                pak_paths.push_back(entry.path());
        }
    }
    struct Loaded { int priority; std::size_t index; };
    std::vector<Loaded> loaded;
    for (const auto& path : pak_paths) {
        auto archive = PakArchive::open_from_file(path);
        if (!archive) return archive.error();
        const int n = update_number(path.filename().string());
        const int priority = n < 0 ? 0 : 1000 + n;
        ArchiveInfo info{path.filename().string(), archive->entries().size(), n >= 0, n};
        vfs.paks_.push_back(std::make_unique<PakArchive>(std::move(archive.value())));
        vfs.archives_.push_back(info);
        loaded.push_back({priority, vfs.paks_.size() - 1});
    }
    std::sort(loaded.begin(), loaded.end(), [](const Loaded& a, const Loaded& b) {
        return a.priority < b.priority;
    });
    for (const auto& item : loaded) {
        const PakArchive& pak = *vfs.paks_[item.index];
        for (const auto& entry : pak.entries()) {
            const std::string key = pak_entry_key(entry.name);
            vfs.index_[key] = Location{&pak, &entry, vfs.archives_[item.index].name};
        }
    }
    return vfs;
}

bool GameVfs::exists(std::string_view vp) const {
    if (loose_path(vp)) return true;
    return index_.find(normalize_virtual_path(vp)) != index_.end();
}

Result<Bytes> GameVfs::read(std::string_view vp) const {
    if (const auto loose = loose_path(vp)) {
        std::ifstream in(*loose, std::ios::binary | std::ios::ate);
        if (!in) return Error{ErrorCode::io, "cannot open loose file"};
        const auto size = in.tellg();
        if (size < 0) return Error{ErrorCode::io, "cannot size loose file"};
        in.seekg(0);
        Bytes out(static_cast<std::size_t>(size));
        if (!in.read(reinterpret_cast<char*>(out.data()), out.size()))
            return Error{ErrorCode::io, "cannot read loose file"};
        return out;
    }
    const auto it = index_.find(normalize_virtual_path(vp));
    if (it == index_.end()) return Error{ErrorCode::io, "file not found: " + std::string(vp)};
    return it->second.pak->read(*it->second.entry);
}

std::vector<VfsEntry> GameVfs::list() const {
    std::vector<VfsEntry> out;
    out.reserve(index_.size());
    for (const auto& [key, loc] : index_)
        out.push_back({key, loc.entry->size, loc.source});
    return out;
}

const PakArchive* GameVfs::archive(std::string_view file_name) const {
    for (std::size_t i = 0; i < archives_.size(); ++i)
        if (archives_[i].name == file_name) return paks_[i].get();
    return nullptr;
}
}
