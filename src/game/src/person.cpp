#include "gl/game/person.hpp"

#include "gl/assets/csx.hpp"
#include "gl/assets/log.hpp"
#include "gl/assets/sef.hpp"

#include <array>
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

void warn(std::string_view message) { gl::log(gl::LogLevel::warning, message); }

std::string lower_path(std::string_view text) {
    std::string out(text);
    for (char &c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        else if (c == '\\')
            c = '/';
    }
    return out;
}

std::string sef_path(std::string_view name) {
    std::string path = "levels/single/";
    path.append(name);
    path.push_back('/');
    path.append(name);
    path += ".sef";
    return path;
}

std::string script_path(std::string_view name) {
    return "scripts/persons/" + lower_path(name) + ".scr";
}

std::string person_directory(std::string_view res_name) {
    return "persons/" + lower_path(res_name);
}

std::uint8_t direction_index(std::string_view direction, bool &known) {
    static constexpr std::array<std::string_view, 8> kDirections{
        "UP", "UP_LEFT", "LEFT", "DOWN_LEFT", "DOWN", "DOWN_RIGHT", "RIGHT", "UP_RIGHT"};
    std::string key(direction);
    for (char &c : key) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
        else if (c == '-' || c == ' ')
            c = '_';
    }
    for (std::size_t i = 0; i < kDirections.size(); ++i) {
        if (key == kDirections[i]) {
            known = true;
            return static_cast<std::uint8_t>(i);
        }
    }
    known = false;
    return 0;
}

Image to_image(const CsxImage &csx) {
    Image image;
    image.width = csx.width;
    image.height = csx.height;
    image.rgba = csx.rgba;
    return image;
}

bool decode_person_image(GameVfs &vfs, const std::string &path, Image &out, bool required,
                         std::string_view level, std::string_view person) {
    auto data = vfs.read(path);
    if (!data) {
        if (required) {
            warn("level '" + std::string(level) + "': skipping person '" + std::string(person) +
                 "': " + contextual(data.error(), "cannot read sprite", path).message);
        }
        return false;
    }
    auto csx = decode_csx(data.value());
    if (!csx) {
        if (required) {
            warn("level '" + std::string(level) + "': skipping person '" + std::string(person) +
                 "': " + contextual(csx.error(), "cannot decode sprite", path).message);
        }
        return false;
    }
    out = to_image(csx.value());
    return true;
}
} // namespace

Result<std::vector<PersonInstance>> load_persons(GameVfs &vfs, std::string_view level_name) {
    try {
        if (level_name.empty())
            return Error{ErrorCode::argument, "level name is empty"};

        const std::string sef_file = sef_path(level_name);
        auto sef_data = vfs.read(sef_file);
        if (!sef_data)
            return Error{sef_data.error().code,
                         "cannot read level SEF '" + sef_file + "': " + sef_data.error().message};
        auto sef = parse_sef(sef_data.value());
        if (!sef)
            return Error{ErrorCode::format,
                         "cannot parse level SEF '" + sef_file + "': " + sef.error().message};

        std::vector<PersonInstance> persons;
        persons.reserve(sef->persons.size());

        for (const SefPerson &source : sef->persons) {
            const std::string script_file = script_path(source.name);
            auto script_data = vfs.read(script_file);
            if (!script_data) {
                warn("level '" + std::string(level_name) + "': skipping person '" + source.name +
                     "': " +
                     contextual(script_data.error(), "cannot read person script", script_file)
                         .message);
                continue;
            }
            auto def = parse_person_script(script_data.value());
            if (!def) {
                warn("level '" + std::string(level_name) + "': skipping person '" + source.name +
                     "': " +
                     contextual(def.error(), "cannot parse person script", script_file).message);
                continue;
            }

            const std::string directory = person_directory(def->res_name);
            const std::string pad_file = directory + "/" + lower_path(def->res_name) + ".pad";
            auto pad_data = vfs.read(pad_file);
            if (!pad_data) {
                warn("level '" + std::string(level_name) + "': skipping person '" + source.name +
                     "': " +
                     contextual(pad_data.error(), "cannot read person PAD", pad_file).message);
                continue;
            }
            auto pad = parse_pad(pad_data.value());
            if (!pad) {
                warn("level '" + std::string(level_name) + "': skipping person '" + source.name +
                     "': " + contextual(pad.error(), "cannot parse person PAD", pad_file).message);
                continue;
            }

            const PadAnimation *stay = nullptr;
            for (const PadAnimation &animation : pad.value()) {
                if (animation.type == PadAnimationType::rt_stay) {
                    stay = &animation;
                    break;
                }
            }
            if (stay == nullptr) {
                for (const PadAnimation &animation : pad.value()) {
                    if (animation.type == PadAnimationType::tb_stay) {
                        stay = &animation;
                        break;
                    }
                }
            }
            if (stay == nullptr) {
                warn("level '" + std::string(level_name) + "': skipping person '" + source.name +
                     "': PAD has neither rt_stay nor tb_stay animation");
                continue;
            }
            const std::string sheet_name =
                stay->type == PadAnimationType::tb_stay ? "tb_stay" : "rt_stay";

            PersonInstance person;
            person.stay_type = stay->type;
            if (!decode_person_image(vfs, directory + "/animation/" + sheet_name + ".csx",
                                     person.sprite, true, level_name, source.name)) {
                continue;
            }
            decode_person_image(vfs, directory + "/shadows/" + sheet_name + ".csx", person.shadow,
                                false, level_name, source.name);

            person.name = source.name;
            person.tile_x = source.x;
            person.tile_y = source.y;
            bool known_direction = false;
            person.direction = direction_index(source.direction, known_direction);
            if (!known_direction) {
                warn("level '" + std::string(level_name) + "': person '" + source.name +
                     "' has unknown direction '" + source.direction + "', using UP");
            }
            person.route_type = source.route_type;
            person.route = source.route;
            person.radius = source.radius;
            person.delay_min = source.delay_min;
            person.delay_max = source.delay_max;
            person.tribe = source.tribe;
            person.dialog = source.dialog;
            person.inventory = source.inventory;
            person.res_name = def->res_name;
            person.def = std::move(def.value());
            person.pad = std::move(pad.value());
            persons.push_back(std::move(person));
        }

        return persons;
    } catch (const std::bad_alloc &) {
        return Error{ErrorCode::format, "person allocation failed"};
    }
}
} // namespace gl::game
