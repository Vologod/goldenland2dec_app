#include "gl/assets/csx.hpp"
#include "gl/assets/csx_frames.hpp"
#include "gl/assets/lao.hpp"
#include "gl/assets/log.hpp"
#include "gl/assets/lvl.hpp"
#include "gl/assets/sef.hpp"
#include "gl/game/level.hpp"
#include "gl/game/person.hpp"
#include <algorithm>
#include <cstddef>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gl::game {
namespace {
using namespace gl::assets;

Error contextual(Error error, std::string_view action, std::string_view path) {
    return Error{error.code,
                 std::string(action) + " '" + std::string(path) + "': " + error.message};
}

Error required(Error error, std::string_view action, std::string_view path) {
    error.code = ErrorCode::format;
    return contextual(error, action, path);
}

void warn(std::string_view message) { gl::log(gl::LogLevel::warning, message); }

std::string sef_path(std::string_view name) {
    std::string path = "levels/single/";
    path.append(name);
    path.push_back('/');
    path.append(name);
    path += ".sef";
    return path;
}

std::string lvl_path(std::string_view pack) {
    std::string path = "levels/lvl/";
    path.append(pack);
    path += ".lvl";
    return path;
}

std::string pack_path(std::string_view pack, std::string_view relative) {
    std::string path = "levels/pack/";
    path.append(pack);
    path.push_back('/');
    path.append(relative);
    return path;
}

Image to_image(const CsxImage &csx) {
    Image image;
    image.width = csx.width;
    image.height = csx.height;
    image.rgba = csx.rgba;
    return image;
}
} // namespace

Result<Level> load_level(GameVfs &vfs, std::string_view level_name) {
    try {
        if (level_name.empty())
            return Error{ErrorCode::argument, "level name is empty"};

        Level level;
        level.name = std::string(level_name);

        const std::string sef_file = sef_path(level.name);
        auto sef_data = vfs.read(sef_file);
        if (!sef_data)
            return required(sef_data.error(), "cannot read level SEF", sef_file);
        auto sef = parse_sef(sef_data.value());
        if (!sef)
            return contextual(sef.error(), "cannot parse level SEF", sef_file);
        level.pack = sef->pack;
        if (level.pack.empty())
            return Error{ErrorCode::format, "level SEF '" + sef_file + "' has an empty pack"};

        const std::string lvl_file = lvl_path(level.pack);
        auto lvl_data = vfs.read(lvl_file);
        if (!lvl_data)
            return required(lvl_data.error(), "cannot read level LVL", lvl_file);
        auto lvl = parse_lvl(lvl_data.value());
        if (!lvl)
            return contextual(lvl.error(), "cannot parse level LVL", lvl_file);

        const std::string background_file = pack_path(level.pack, "bitmaps/layer.jpg");
        auto background_data = vfs.read(background_file);
        if (!background_data)
            return required(background_data.error(), "cannot read level background",
                            background_file);
        auto background = decode_image(background_data.value());
        if (!background)
            return contextual(background.error(), "cannot decode level background",
                              background_file);
        level.background = std::move(background.value());
        level.width = level.background.width;
        level.height = level.background.height;

        if (level.width != lvl->map_size.pixel_width ||
            level.height != lvl->map_size.pixel_height) {
            warn("level '" + level.name + "': background size " + std::to_string(level.width) +
                 "x" + std::to_string(level.height) + " does not match LVL map size " +
                 std::to_string(lvl->map_size.pixel_width) + "x" +
                 std::to_string(lvl->map_size.pixel_height));
        }

        std::vector<LvlDescription> static_descriptions = lvl->statics;
        std::stable_sort(
            static_descriptions.begin(), static_descriptions.end(),
            [](const LvlDescription &a, const LvlDescription &b) { return a.number < b.number; });

        level.statics.reserve(static_descriptions.size());
        for (const auto &description : static_descriptions) {
            const std::string path = pack_path(
                level.pack, "bitmaps/static/static_" + std::to_string(description.number) + ".csx");
            auto data = vfs.read(path);
            if (!data) {
                warn("level '" + level.name +
                     "': " + contextual(data.error(), "skipping static", path).message);
                continue;
            }
            auto csx = decode_csx(data.value());
            if (!csx) {
                warn("level '" + level.name +
                     "': " + contextual(csx.error(), "skipping static", path).message);
                continue;
            }
            LevelStatic object;
            object.number = description.number;
            object.x = description.x;
            object.y = description.y;
            object.image = to_image(csx.value());
            level.statics.push_back(std::move(object));
        }

        std::vector<LaoInfo> lao;
        if (!lvl->animations.empty()) {
            const std::string lao_file =
                pack_path(level.pack, "data/animated/" + level.pack + ".lao");
            auto lao_data = vfs.read(lao_file);
            if (!lao_data) {
                warn("level '" + level.name +
                     "': " + contextual(lao_data.error(), "skipping animations", lao_file).message);
            } else {
                auto parsed = parse_lao(lao_data.value());
                if (!parsed) {
                    warn("level '" + level.name + "': " +
                         contextual(parsed.error(), "skipping animations", lao_file).message);
                } else {
                    lao = std::move(parsed.value());
                }
            }
        }

        if (!lao.empty()) {
            std::vector<LvlDescription> descriptions = lvl->animations;
            std::stable_sort(descriptions.begin(), descriptions.end(),
                             [](const LvlDescription &a, const LvlDescription &b) {
                                 return a.number < b.number;
                             });

            if (descriptions.size() != lao.size()) {
                warn("level '" + level.name +
                     "': animation count mismatch: " + std::to_string(descriptions.size()) +
                     " descriptions, " + std::to_string(lao.size()) + " LAO entries");
            }

            const std::size_t count = std::min(descriptions.size(), lao.size());
            level.animations.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                const std::string path =
                    pack_path(level.pack, "bitmaps/animated/anim_" + std::to_string(i) + ".csx");
                auto data = vfs.read(path);
                if (!data) {
                    warn("level '" + level.name +
                         "': " + contextual(data.error(), "skipping animation", path).message);
                    continue;
                }
                auto csx = decode_csx(data.value());
                if (!csx) {
                    warn("level '" + level.name +
                         "': " + contextual(csx.error(), "skipping animation", path).message);
                    continue;
                }
                auto frames = slice_csx_frames(csx.value(), lao[i].height);
                if (!frames) {
                    warn("level '" + level.name +
                         "': " + contextual(frames.error(), "skipping animation", path).message);
                    continue;
                }

                LevelAnimation animation;
                animation.number = descriptions[i].number;
                animation.x = descriptions[i].x;
                animation.y = descriptions[i].y;
                animation.delay_ms = lao[i].delay;
                animation.frames = std::move(frames.value());
                level.animations.push_back(std::move(animation));
            }
        }

        auto persons = load_persons(vfs, level.name);
        if (!persons)
            return contextual(persons.error(), "cannot load level persons", sef_file);
        level.persons = std::move(persons.value());

        return level;
    } catch (const std::bad_alloc &) {
        return Error{ErrorCode::format, "level allocation failed"};
    }
}
} // namespace gl::game
