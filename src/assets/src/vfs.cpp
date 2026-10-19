#include "gl/assets/vfs.hpp"
#include "gl/assets/cp1251.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

namespace gl::assets {
namespace {
bool has_pak_extension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return ext == ".pak";
}

int update_number(const std::string& filename) {
    const std::string marker = ".update.";
    auto pos = filename.rfind(marker);
    if (pos == std::string::npos) return -1;
    const std::size_t begin = pos + marker.size();
    const auto end = filename.find(".pak", begin);
    if (end == std::string::npos || begin == end) return -1;
    int value = 0;
    for (std::size_t i = begin; i < end; ++i) {
        if (filename[i] < '0' || filename[i] > '9') return -1;
        const int digit = filename[i] - '0';
        if (value > (std::numeric_limits<int>::max() - digit) / 10) return -1;
        value = value * 10 + digit;
    }
    return value;
}

void collect_pak_paths(const std::filesystem::path& dir,
                       std::vector<std::filesystem::path>& out) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code entry_ec;
        if (it->is_regular_file(entry_ec) && has_pak_extension(it->path()))
            out.push_back(it->path());
    }
}

void collect_loose_files(const std::filesystem::path& root,
                         const std::filesystem::path& base,
                         bool skip_data_subtree,
                         bool overwrite,
                         std::map<std::string, std::filesystem::path, std::less<>>& out) {
    std::error_code ec;
    if (!std::filesystem::is_directory(base, ec)) return;
    std::vector<std::pair<std::string, std::filesystem::path>> found;
    for (std::filesystem::recursive_directory_iterator
             it(base, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end; it.increment(ec)) {
        std::error_code entry_ec;
        const std::filesystem::path path = it->path();
        if (it->is_directory(entry_ec)) {
            if (skip_data_subtree) {
                const auto rel = path.lexically_relative(root);
                if (!rel.empty() && normalize_virtual_path(rel.generic_string()) == "data")
                    it.disable_recursion_pending();
            }
            continue;
        }
        if (!it->is_regular_file(entry_ec) || has_pak_extension(path)) continue;
        const auto rel = path.lexically_relative(root);
        if (rel.empty()) continue;
        std::string key = normalize_virtual_path(rel.generic_string());
        if (key.empty()) continue;
        found.emplace_back(std::move(key), path);
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second.generic_string() < b.second.generic_string();
    });
    for (auto& [key, path] : found) {
        if (overwrite) out[key] = std::move(path);
        else out.emplace(std::move(key), std::move(path));
    }
}
}

std::optional<std::filesystem::path> GameVfs::loose_path(std::string_view vp) const {
    if (!is_safe_relative_path(vp)) return std::nullopt;
    const std::string key = normalize_virtual_path(vp);
    if (key.empty()) return std::nullopt;
    const auto it = loose_index_.find(key);
    if (it == loose_index_.end()) return std::nullopt;
    return it->second;
}

Result<GameVfs> GameVfs::mount(const std::filesystem::path& game_dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(game_dir, ec))
        return Error{ErrorCode::io, "game directory not found: " + game_dir.string()};
    GameVfs vfs;
    vfs.game_dir_ = game_dir;

    collect_loose_files(game_dir, game_dir, true, true, vfs.loose_index_);
    collect_loose_files(game_dir / "Data", game_dir / "Data", false, false, vfs.loose_index_);

    std::vector<std::filesystem::path> pak_paths;
    collect_pak_paths(game_dir, pak_paths);
    collect_pak_paths(game_dir / "Data", pak_paths);

    struct Loaded {
        std::int64_t priority;
        std::string name;
        std::string path;
        std::size_t index;
    };
    std::vector<Loaded> loaded;
    for (const auto& path : pak_paths) {
        auto archive = PakArchive::open_from_file(path);
        if (!archive) return archive.error();
        const int n = update_number(path.filename().string());
        const std::int64_t priority =
            n < 0 ? 0 : 1000 + static_cast<std::int64_t>(n);
        ArchiveInfo info{path.filename().string(), archive->entries().size(), n >= 0, n};
        vfs.paks_.push_back(std::make_unique<PakArchive>(std::move(archive.value())));
        vfs.archives_.push_back(info);
        loaded.push_back({priority, info.name, path.generic_string(), vfs.paks_.size() - 1});
    }
    std::sort(loaded.begin(), loaded.end(), [](const Loaded& a, const Loaded& b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        if (a.name != b.name) return a.name < b.name;
        return a.path < b.path;
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
        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(*loose, ec);
        if (ec) return Error{ErrorCode::io, "cannot size loose file"};
        if (size > std::numeric_limits<std::size_t>::max())
            return Error{ErrorCode::io, "loose file too large"};
        std::ifstream in(*loose, std::ios::binary);
        if (!in) return Error{ErrorCode::io, "cannot open loose file"};
        Bytes out(static_cast<std::size_t>(size));
        if (!out.empty() &&
            !in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size())))
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
