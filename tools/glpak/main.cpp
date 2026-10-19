#include "gl/assets/cp1251.hpp"
#include "gl/assets/csx.hpp"
#include "gl/assets/image.hpp"
#include "gl/assets/itm.hpp"
#include "gl/assets/pak.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using gl::assets::Bytes;
using gl::assets::GameVfs;
using gl::assets::Image;
using gl::assets::Item;
using gl::assets::PakArchive;
using gl::assets::Sdb;

enum ExitCode : int { kOk = 0, kArgument = 1, kIo = 2, kFormat = 3, kMismatch = 4 };

int fail(const std::string& message, int code) {
    std::cerr << "glpak: " << message << '\n';
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
                 "  glpak info <game_dir>\n"
                 "  glpak list <game_dir> [--archive <name>]\n"
                 "  glpak extract <game_dir> --all --out <dir>\n"
                 "  glpak extract <game_dir> --archive <name> --out <dir>\n"
                 "  glpak extract <game_dir> --file <virtual/path> --out <dir>\n"
                 "  glpak extract <game_dir> --prefix <prefix> --out <dir>\n"
                 "  glpak decode-csx <game_dir> --file <path> --out <png>\n"
                 "  glpak decode-sdb <game_dir> --file <path> --out <txt>\n"
                 "  glpak decode-itm <game_dir> --file <path>\n";
}

bool ensure_parent(const fs::path& path, std::string& error) {
    const fs::path parent = path.parent_path();
    if (parent.empty()) return true;
    std::error_code ec;
    fs::create_directories(parent, ec);
    if (ec) {
        error = "cannot create directory " + parent.string() + ": " + ec.message();
        return false;
    }
    return true;
}

int write_bytes(const fs::path& path, const Bytes& data) {
    std::string error;
    if (!ensure_parent(path, error)) return fail(error, kIo);
    std::ofstream out(path, std::ios::binary);
    if (!out) return fail("cannot open for writing: " + path.string(), kIo);
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }
    if (!out) return fail("cannot write: " + path.string(), kIo);
    return kOk;
}

int write_text(const fs::path& path, const std::string& text) {
    std::string error;
    if (!ensure_parent(path, error)) return fail(error, kIo);
    std::ofstream out(path, std::ios::binary);
    if (!out) return fail("cannot open for writing: " + path.string(), kIo);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) return fail("cannot write: " + path.string(), kIo);
    return kOk;
}

std::optional<fs::path> extraction_target(const fs::path& out_dir, std::string_view raw_path) {
    if (!gl::assets::is_safe_relative_path(raw_path)) return std::nullopt;
    const std::string normalized = gl::assets::normalize_virtual_path(raw_path);
    if (normalized.empty() || !gl::assets::is_safe_relative_path(normalized)) return std::nullopt;
    return out_dir / fs::path(normalized);
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

bool set_once(std::optional<std::string>& slot, const std::string& name,
              std::string value, std::string& error) {
    if (slot) {
        error = "duplicate option " + name;
        return false;
    }
    slot = std::move(value);
    return true;
}

struct ExtractOptions {
    bool all = false;
    std::optional<std::string> archive;
    std::optional<std::string> file;
    std::optional<std::string> prefix;
    std::optional<std::string> out;
};

struct ExtractStats {
    std::size_t written = 0;
    bool format_error = false;
    bool io_error = false;
};

bool extract_batch_entry(const fs::path& out_dir, std::string_view virtual_path,
                         gl::Result<Bytes> data, ExtractStats& stats) {
    if (!data) {
        std::cerr << "glpak: " << virtual_path << ": " << data.error().message << '\n';
        if (data.error().code == gl::ErrorCode::format ||
            data.error().code == gl::ErrorCode::unsupported)
            stats.format_error = true;
        else
            stats.io_error = true;
        return true;
    }
    const auto target = extraction_target(out_dir, virtual_path);
    if (!target) {
        std::cerr << "glpak: " << virtual_path << ": unsafe virtual path\n";
        stats.format_error = true;
        return true;
    }
    const int code = write_bytes(*target, data.value());
    if (code != kOk) return false;
    ++stats.written;
    return true;
}

bool parse_extract_options(const std::vector<std::string>& args, ExtractOptions& options,
                           std::string& error) {
    for (std::size_t i = 3; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--all") {
            if (options.all) {
                error = "duplicate option --all";
                return false;
            }
            options.all = true;
            continue;
        }
        if (arg == "--archive" || arg == "--file" || arg == "--prefix" || arg == "--out") {
            std::string value;
            if (!option_value(args, i, arg, value, error)) return false;
            if (arg == "--archive") {
                if (!set_once(options.archive, arg, std::move(value), error)) return false;
            } else if (arg == "--file") {
                if (!set_once(options.file, arg, std::move(value), error)) return false;
            } else if (arg == "--prefix") {
                if (!set_once(options.prefix, arg, std::move(value), error)) return false;
            } else if (!set_once(options.out, arg, std::move(value), error)) {
                return false;
            }
            continue;
        }
        error = "unknown option: " + arg;
        return false;
    }
    const int selectors = (options.all ? 1 : 0) + (options.archive ? 1 : 0) +
                          (options.file ? 1 : 0) + (options.prefix ? 1 : 0);
    if (selectors != 1) {
        error = "extract needs exactly one of --all, --archive, --file, --prefix";
        return false;
    }
    if (!options.out) {
        error = "extract needs --out <dir>";
        return false;
    }
    return true;
}

bool parse_list_options(const std::vector<std::string>& args,
                        std::optional<std::string>& archive, std::string& error) {
    for (std::size_t i = 3; i < args.size(); ++i) {
        if (args[i] != "--archive") {
            error = "unknown option: " + args[i];
            return false;
        }
        std::string value;
        if (!option_value(args, i, "--archive", value, error)) return false;
        if (!set_once(archive, "--archive", std::move(value), error)) return false;
    }
    return true;
}

struct DecodeOptions {
    std::optional<std::string> file;
    std::optional<std::string> out;
};

bool parse_decode_options(const std::vector<std::string>& args, bool require_out,
                          DecodeOptions& options, std::string& error) {
    for (std::size_t i = 3; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--file") {
            std::string value;
            if (!option_value(args, i, arg, value, error)) return false;
            if (!set_once(options.file, arg, std::move(value), error)) return false;
        } else if (require_out && arg == "--out") {
            std::string value;
            if (!option_value(args, i, arg, value, error)) return false;
            if (!set_once(options.out, arg, std::move(value), error)) return false;
        } else {
            error = "unknown option: " + arg;
            return false;
        }
    }
    if (!options.file) {
        error = "missing --file";
        return false;
    }
    if (require_out && !options.out) {
        error = "missing --out";
        return false;
    }
    return true;
}

int cmd_info(const std::vector<std::string>& args) {
    if (args.size() != 3) return fail("usage: glpak info <game_dir>", kArgument);
    auto mounted = GameVfs::mount(fs::path(args[2]));
    if (!mounted) return fail(mounted.error());
    for (const auto& archive : mounted.value().archives()) {
        std::cout << archive.name << '\t' << archive.entry_count << '\t'
                  << (archive.is_update ? "update" : "base");
        if (archive.is_update) std::cout << '\t' << archive.update_number;
        std::cout << '\n';
    }
    return kOk;
}

int cmd_list(const std::vector<std::string>& args) {
    if (args.size() < 3) return fail("usage: glpak list <game_dir> [--archive <name>]", kArgument);
    std::optional<std::string> archive_name;
    std::string error;
    if (!parse_list_options(args, archive_name, error)) return fail(error, kArgument);
    auto mounted = GameVfs::mount(fs::path(args[2]));
    if (!mounted) return fail(mounted.error());
    if (archive_name) {
        const PakArchive* archive = mounted.value().archive(*archive_name);
        if (!archive) return fail("unknown archive: " + *archive_name, kArgument);
        for (const auto& entry : archive->entries())
            std::cout << gl::assets::normalize_virtual_path(entry.name) << '\n';
    } else {
        for (const auto& entry : mounted.value().list()) std::cout << entry.path << '\n';
    }
    return kOk;
}

int cmd_extract(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        return fail("usage: glpak extract <game_dir> <selector> --out <dir>", kArgument);
    }
    ExtractOptions options;
    std::string error;
    if (!parse_extract_options(args, options, error)) return fail(error, kArgument);
    auto mounted = GameVfs::mount(fs::path(args[2]));
    if (!mounted) return fail(mounted.error());
    const GameVfs& vfs = mounted.value();
    const fs::path out_dir(*options.out);
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        return fail("cannot create output directory: " + out_dir.string() + ": " + ec.message(),
                    kIo);
    }

    ExtractStats stats;
    if (options.all) {
        for (const auto& entry : vfs.list()) {
            if (!extract_batch_entry(out_dir, entry.path, vfs.read(entry.path), stats))
                return stats.format_error ? kFormat : kIo;
        }
    } else if (options.archive) {
        const PakArchive* archive = vfs.archive(*options.archive);
        if (!archive) return fail("unknown archive: " + *options.archive, kArgument);
        for (const auto& entry : archive->entries()) {
            if (!extract_batch_entry(out_dir, entry.name, archive->read(entry), stats))
                return stats.format_error ? kFormat : kIo;
        }
    } else if (options.file) {
        if (!gl::assets::is_safe_relative_path(*options.file)) {
            return fail("unsafe virtual path: " + *options.file, kArgument);
        }
        const auto target = extraction_target(out_dir, *options.file);
        if (!target) return fail("empty virtual path: " + *options.file, kArgument);
        auto data = vfs.read(*options.file);
        if (!data) return fail(data.error());
        const int code = write_bytes(*target, data.value());
        if (code != kOk) return code;
        ++stats.written;
    } else if (options.prefix) {
        if (!gl::assets::is_safe_relative_path(*options.prefix)) {
            return fail("unsafe prefix: " + *options.prefix, kArgument);
        }
        const std::string prefix = gl::assets::normalize_virtual_path(*options.prefix);
        if (prefix.empty()) return fail("empty prefix", kArgument);
        for (const auto& entry : vfs.list()) {
            if (!entry.path.starts_with(prefix)) continue;
            if (!extract_batch_entry(out_dir, entry.path, vfs.read(entry.path), stats))
                return stats.format_error ? kFormat : kIo;
        }
    }
    std::cout << "extracted " << stats.written << " file(s)\n";
    if (stats.format_error) return kFormat;
    if (stats.io_error) return kIo;
    return kOk;
}

int cmd_decode_csx(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        return fail("usage: glpak decode-csx <game_dir> --file <path> --out <png>", kArgument);
    }
    DecodeOptions options;
    std::string error;
    if (!parse_decode_options(args, true, options, error)) return fail(error, kArgument);
    auto mounted = GameVfs::mount(fs::path(args[2]));
    if (!mounted) return fail(mounted.error());
    auto data = mounted.value().read(*options.file);
    if (!data) return fail(data.error());
    auto decoded = gl::assets::decode_csx(data.value());
    if (!decoded) return fail(decoded.error());
    Image image;
    image.width = decoded.value().width;
    image.height = decoded.value().height;
    image.rgba = decoded.value().rgba;
    const fs::path out(*options.out);
    if (!ensure_parent(out, error)) return fail(error, kIo);
    if (!gl::assets::write_png(out, image)) {
        return fail("cannot write PNG: " + out.string(), kIo);
    }
    std::cout << "wrote " << out.string() << " (" << image.width << 'x' << image.height << ")\n";
    return kOk;
}

int cmd_decode_sdb(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        return fail("usage: glpak decode-sdb <game_dir> --file <path> --out <txt>", kArgument);
    }
    DecodeOptions options;
    std::string error;
    if (!parse_decode_options(args, true, options, error)) return fail(error, kArgument);
    auto mounted = GameVfs::mount(fs::path(args[2]));
    if (!mounted) return fail(mounted.error());
    auto data = mounted.value().read(*options.file);
    if (!data) return fail(data.error());
    auto sdb = Sdb::parse(data.value());
    if (!sdb) return fail(sdb.error());
    std::string text;
    for (const auto& entry : sdb.value().entries()) {
        text += std::to_string(entry.id);
        text.push_back('\t');
        text += entry.value;
        text.push_back('\n');
    }
    const int code = write_text(fs::path(*options.out), text);
    if (code != kOk) return code;
    std::cout << "wrote " << *options.out << " (" << sdb.value().entries().size()
              << " entries)\n";
    return kOk;
}

template <std::size_t N>
void print_array_field(const char* name, const std::array<std::uint32_t, N>& values) {
    std::cout << name << '=';
    for (std::size_t i = 0; i < N; ++i) {
        if (i != 0) std::cout << ',';
        std::cout << values[i];
    }
    std::cout << '\n';
}

void print_item(const Item& item) {
    std::cout << "description_id=" << item.description_id << '\n';
    std::cout << "type=" << static_cast<std::uint32_t>(item.type) << '\n';
    std::cout << "flags=" << item.flags << '\n';
    std::cout << "max_stack=" << item.max_stack << '\n';
    std::cout << "puppet_name=" << item.puppet_name << '\n';
    std::cout << "icon_name=" << item.icon_name << '\n';
    std::cout << "unknown=" << item.unknown << '\n';
    std::cout << "price=" << item.price << '\n';
    std::cout << "weight_x10=" << item.weight_x10 << '\n';
    std::cout << "unknown_a=" << item.unknown_a << '\n';
    print_array_field("unknown_b", item.unknown_b);
    std::cout << "material=" << item.material << '\n';
    std::cout << "shell=" << item.shell << '\n';
    std::cout << "level=" << item.level << '\n';
    std::cout << "strength=" << item.strength << '\n';
    std::cout << "wisdom=" << item.wisdom << '\n';
    std::cout << "endurance=" << item.endurance << '\n';
    std::cout << "intelligence=" << item.intelligence << '\n';
    std::cout << "perception=" << item.perception << '\n';
    std::cout << "agility=" << item.agility << '\n';
    if (item.has_damage) print_array_field("damage", item.damage);
    if (item.has_spell) std::cout << "spell_id=" << item.spell_id << '\n';
    if (item.has_charge) std::cout << "charge=" << item.charge << '\n';
    if (item.has_healing) {
        std::cout << "healing_health=" << item.healing_health << '\n';
        std::cout << "healing_mana=" << item.healing_mana << '\n';
    }
    if (item.has_recipe) print_array_field("recipe", item.recipe);
    if (item.has_damage_type) std::cout << "damage_type=" << item.damage_type << '\n';
    std::cout << "effects=" << item.effects.size() << '\n';
    for (std::size_t i = 0; i < item.effects.size(); ++i) {
        const auto& effect = item.effects[i];
        std::cout << "effect[" << i << "].type=" << effect.type << '\n';
        std::cout << "effect[" << i << "].amount=" << effect.amount << '\n';
        std::cout << "effect[" << i << "].flags=" << effect.flags << '\n';
        std::cout << "effect[" << i << "].duration=" << effect.duration.turns() << '\n';
        std::cout << "effect[" << i << "].delay=" << effect.delay.turns() << '\n';
    }
}

int cmd_decode_itm(const std::vector<std::string>& args) {
    if (args.size() < 3) return fail("usage: glpak decode-itm <game_dir> --file <path>", kArgument);
    DecodeOptions options;
    std::string error;
    if (!parse_decode_options(args, false, options, error)) return fail(error, kArgument);
    auto mounted = GameVfs::mount(fs::path(args[2]));
    if (!mounted) return fail(mounted.error());
    auto data = mounted.value().read(*options.file);
    if (!data) return fail(data.error());
    auto item = gl::assets::parse_item(data.value());
    if (!item) return fail(item.error());
    print_item(item.value());
    return kOk;
}

int run(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) args.emplace_back(argv[i]);
    if (args.size() < 2) {
        print_usage();
        return kArgument;
    }
    const std::string& command = args[1];
    if (command == "info") return cmd_info(args);
    if (command == "list") return cmd_list(args);
    if (command == "extract") return cmd_extract(args);
    if (command == "decode-csx") return cmd_decode_csx(args);
    if (command == "decode-sdb") return cmd_decode_sdb(args);
    if (command == "decode-itm") return cmd_decode_itm(args);
    print_usage();
    return fail("unknown command: " + command, kArgument);
}

}

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        return fail(std::string("unexpected error: ") + error.what(), kIo);
    } catch (...) {
        return fail("unexpected error", kIo);
    }
}
