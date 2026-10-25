#include "gl/game/hero_wear.hpp"
#include "gl/assets/csx.hpp"
#include "gl/assets/log.hpp"

#include <algorithm>
#include <array>

namespace gl::game {
namespace {
void blit(const gl::assets::Image &src, gl::assets::Image &dst, int dx, int dy, int sx, int sy,
          int w, int h) {
    if (w <= 0 || h <= 0)
        return;
    const int x0 = std::max(0, -dx);
    const int y0 = std::max(0, -dy);
    const int x1 = std::min(w, static_cast<int>(dst.width) - dx);
    const int y1 = std::min(h, static_cast<int>(dst.height) - dy);
    if (x0 >= x1 || y0 >= y1)
        return;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const int ssx = sx + x;
            const int ssy = sy + y;
            if (ssx < 0 || ssy < 0 || ssx >= static_cast<int>(src.width) ||
                ssy >= static_cast<int>(src.height))
                continue;
            const gl::assets::Rgba p = src.rgba[static_cast<std::size_t>(ssy) * src.width + ssx];
            if (p.a == 0)
                continue;
            dst.rgba[static_cast<std::size_t>(dy + y) * dst.width + (dx + x)] = p;
        }
    }
}

void draw_body_layer(const gl::assets::WearHadAnimation &a, std::uint32_t col, std::uint32_t row,
                     const WearLayerImages &layer, gl::assets::Image &dst) {
    if (layer.block == nullptr || layer.body == nullptr || layer.seq == nullptr)
        return;
    const auto &b = *layer.block;
    const std::size_t index = static_cast<std::size_t>(row) * a.columns + col;
    if (index >= b.crops.size())
        return;
    const auto crop = b.crops[index];
    const int dx =
        static_cast<int>(col * a.frame_width) + b.layer_offset_x + crop.x1 - a.layer_offset_x;
    const int dy =
        static_cast<int>(row * a.frame_height) + b.layer_offset_y + crop.y1 - a.layer_offset_y;
    const int sx = static_cast<int>(col * b.frame_width) + crop.x1;
    const int sy = static_cast<int>(row * b.frame_height) + crop.y1;
    blit(*layer.body, dst, dx, dy, sx, sy, crop.x2 - crop.x1, crop.y2 - crop.y1);
}

void draw_shadow_layer(const gl::assets::WearHadAnimation &a, std::uint32_t col,
                       std::uint32_t shadow_row, const WearLayerImages &layer,
                       gl::assets::Image &dst) {
    if (layer.block == nullptr || layer.shadow == nullptr)
        return;
    const auto &b = *layer.block;
    const std::size_t index = static_cast<std::size_t>(shadow_row) * a.columns + col;
    if (index >= b.shadow_crops.size())
        return;
    const auto crop = b.shadow_crops[index];
    const int dx = static_cast<int>(col * b.shadow_frame_width) + b.shadow_layer_offset_x +
                   crop.x1 - a.shadow_offset_x;
    const int dy = static_cast<int>(shadow_row * b.shadow_frame_height) + b.shadow_layer_offset_y +
                   crop.y1 - a.shadow_offset_y;
    const int sx = static_cast<int>(col * b.shadow_frame_width) + crop.x1;
    const int sy = static_cast<int>(shadow_row * b.shadow_frame_height) + crop.y1;
    blit(*layer.shadow, dst, dx, dy, sx, sy, crop.x2 - crop.x1, crop.y2 - crop.y1);
}
} // namespace

gl::assets::Image compose_wear_body(const gl::assets::WearHadAnimation &animation,
                                    const gl::assets::Image &base,
                                    std::span<const WearLayerImages> layers) {
    gl::assets::Image out;
    out.width = animation.columns * animation.frame_width;
    out.height = animation.rows * animation.frame_height;
    out.rgba.assign(static_cast<std::size_t>(out.width) * out.height, gl::assets::Rgba{});
    for (std::uint32_t row = 0; row < animation.rows; ++row) {
        for (std::uint32_t col = 0; col < animation.columns; ++col) {
            const std::size_t index = static_cast<std::size_t>(row) * animation.columns + col;
            for (std::uint32_t order = 0; order <= 4; ++order) {
                for (const WearLayerImages &layer : layers) {
                    if (layer.seq == nullptr || index >= layer.seq->order.size())
                        continue;
                    if (layer.seq->order[index] == order)
                        draw_body_layer(animation, col, row, layer, out);
                }
                if (order == 1) {
                    blit(base, out, static_cast<int>(col * animation.frame_width),
                         static_cast<int>(row * animation.frame_height),
                         static_cast<int>(col * animation.frame_width),
                         static_cast<int>(row * animation.frame_height),
                         static_cast<int>(animation.frame_width),
                         static_cast<int>(animation.frame_height));
                }
            }
        }
    }
    return out;
}

gl::assets::Image compose_wear_shadow(const gl::assets::WearHadAnimation &animation,
                                      const gl::assets::Image &base,
                                      std::span<const WearLayerImages> layers) {
    gl::assets::Image out;
    out.width = animation.columns * animation.shadow_width;
    out.height = (animation.rows * 2 - 2) * animation.shadow_height;
    out.rgba.assign(static_cast<std::size_t>(out.width) * out.height, gl::assets::Rgba{});
    for (std::uint32_t row = 0; row < animation.rows * 2 - 2; ++row) {
        for (std::uint32_t col = 0; col < animation.columns; ++col) {
            for (const WearLayerImages &layer : layers)
                draw_shadow_layer(animation, col, row, layer, out);
            blit(base, out, static_cast<int>(col * animation.shadow_width),
                 static_cast<int>(row * animation.shadow_height),
                 static_cast<int>(col * animation.shadow_width),
                 static_cast<int>(row * animation.shadow_height),
                 static_cast<int>(animation.shadow_width),
                 static_cast<int>(animation.shadow_height));
        }
    }
    return out;
}

namespace {
bool load_csx(const gl::assets::GameVfs &vfs, const std::string &path, gl::assets::Image &out,
              std::string &reason) {
    auto data = vfs.read(path);
    if (!data) {
        reason = "cannot read " + path;
        return false;
    }
    auto image = gl::assets::decode_csx(data.value());
    if (!image) {
        reason = "cannot decode " + path + ": " + image.error().message;
        return false;
    }
    out.width = image->width;
    out.height = image->height;
    out.rgba = std::move(image->rgba);
    return true;
}

bool check_dims(const gl::assets::Image &image, std::uint64_t w, std::uint64_t h,
                const std::string &path, std::string &reason) {
    if (image.width == w && image.height == h)
        return true;
    reason = path + ": expected " + std::to_string(w) + "x" + std::to_string(h) + ", got " +
             std::to_string(image.width) + "x" + std::to_string(image.height);
    return false;
}

const gl::assets::IadBlock *find_block(const std::vector<gl::assets::IadBlock> &blocks,
                                       std::uint32_t mask) {
    for (const auto &b : blocks)
        if (b.type == mask)
            return &b;
    return nullptr;
}

std::string animation_name(std::uint32_t mask) {
    switch (mask) {
    case 0x1:
        return "rt_stay";
    case 0x2:
        return "rt_fun";
    case 0x4:
        return "tb_stay";
    case 0x8:
        return "tb_fun";
    case 0x10:
        return "tb_go";
    case 0x20:
        return "rt_go";
    case 0x40:
        return "cast";
    case 0x80:
        return "suffer";
    case 0x100:
        return "die";
    case 0x200:
        return "run";
    case 0x400:
        return "ss_attack";
    case 0x10000:
        return "hits0";
    case 0x20000:
        return "hits1";
    case 0x40000:
        return "hits2";
    case 0x80000:
        return "hits3";
    default:
        return {};
    }
}

struct LayerFiles {
    const char *category = nullptr;
    const std::string *item = nullptr;
    std::vector<gl::assets::IadBlock> blocks;
};

bool load_layer_files(const gl::assets::GameVfs &vfs, const std::string &set, LayerFiles &layer) {
    if (layer.item == nullptr || layer.item->empty())
        return false;
    const std::string base = "wear/" + set + "/" + layer.category + "/" + *layer.item + "/";
    auto data = vfs.read(base + *layer.item + ".iad");
    if (!data) {
        gl::log(gl::LogLevel::warning, "wear layer '" + base + "': cannot read IAD");
        return false;
    }
    auto blocks = gl::assets::parse_iad(data.value());
    if (!blocks) {
        gl::log(gl::LogLevel::warning, "wear layer '" + base + "': " + blocks.error().message);
        return false;
    }
    layer.blocks = std::move(blocks.value());
    return true;
}
} // namespace

Result<HeroVisual> load_hero_visual(const gl::assets::GameVfs &vfs, const HeroOutfit &outfit) {
    HeroVisual visual;
    visual.outfit_ = outfit;

    LayerFiles layers[5] = {
        {"weapon", &outfit.weapon, {}},   {"shield", &outfit.shield, {}},
        {"elbarms", &outfit.elbarms, {}}, {"helmet", &outfit.helmet, {}},
        {"armor", &outfit.armor, {}},
    };
    for (auto &layer : layers)
        load_layer_files(vfs, outfit.wear_set, layer);

    const std::string set = "wear/" + outfit.wear_set + "/";
    auto had_data = vfs.read(set + outfit.wear_set + ".had");
    if (!had_data)
        return Error{ErrorCode::io, "cannot read wear HAD for set '" + outfit.wear_set + "'"};
    auto had = gl::assets::parse_wear_had(had_data.value());
    if (!had)
        return had.error();

    for (const auto &header : had.value()) {
        const std::string anim = animation_name(header.type_mask);
        if (anim.empty())
            continue;
        HeroAnimation out;
        out.header = header;
        std::string reason;
        const std::string body_path = set + "animation/" + anim + ".csx";
        if (!load_csx(vfs, body_path, out.body, reason)) {
            gl::log(gl::LogLevel::warning, reason + " (animation skipped)");
            continue;
        }
        if (!check_dims(out.body, static_cast<std::uint64_t>(header.columns) * header.frame_width,
                        static_cast<std::uint64_t>(header.rows) * header.frame_height, body_path,
                        reason))
            return Error{ErrorCode::format, reason};

        const std::string shadow_path = set + "shadows/" + anim + ".csx";
        if (!load_csx(vfs, shadow_path, out.shadow, reason)) {
            gl::log(gl::LogLevel::warning, reason + " (animation skipped)");
            continue;
        }
        if (!check_dims(out.shadow,
                        static_cast<std::uint64_t>(header.columns) * header.shadow_width,
                        static_cast<std::uint64_t>(header.rows * 2 - 2) * header.shadow_height,
                        shadow_path, reason))
            return Error{ErrorCode::format, reason};

        struct LoadedLayer {
            gl::assets::Image body;
            gl::assets::Image shadow;
            gl::assets::WearSequence seq;
            const gl::assets::IadBlock *block = nullptr;
            bool valid = false;
        };
        std::array<LoadedLayer, 5> loaded{};
        for (std::size_t i = 0; i < 5; ++i) {
            const auto &layer = layers[i];
            const gl::assets::IadBlock *block = find_block(layer.blocks, header.type_mask);
            if (block == nullptr)
                continue;
            if (block->frames != header.columns)
                return Error{ErrorCode::format,
                             std::string(layer.category) + " IAD frame count mismatch"};
            if (block->crops.size() < static_cast<std::size_t>(header.rows) * header.columns)
                return Error{ErrorCode::format,
                             std::string(layer.category) + " IAD crop count mismatch"};
            if (block->shadow_crops.size() <
                static_cast<std::size_t>(header.rows * 2 - 2) * header.columns)
                return Error{ErrorCode::format,
                             std::string(layer.category) + " IAD shadow crop count mismatch"};

            const std::string layer_base = set + layer.category + "/" + *layer.item + "/";
            auto &dst = loaded[i];
            if (!load_csx(vfs, layer_base + "animation/" + anim + ".csx", dst.body, reason)) {
                gl::log(gl::LogLevel::warning, reason + " (layer skipped)");
                continue;
            }
            if (!check_dims(dst.body,
                            static_cast<std::uint64_t>(header.columns) * block->frame_width,
                            static_cast<std::uint64_t>(header.rows) * block->frame_height,
                            layer_base + "animation/" + anim + ".csx", reason))
                return Error{ErrorCode::format, reason};

            if (!load_csx(vfs, layer_base + "shadows/" + anim + ".csx", dst.shadow, reason)) {
                gl::log(gl::LogLevel::warning, reason + " (shadow layer skipped)");
                continue;
            }
            if (!check_dims(dst.shadow,
                            static_cast<std::uint64_t>(header.columns) * block->shadow_frame_width,
                            static_cast<std::uint64_t>(header.rows * 2 - 2) *
                                block->shadow_frame_height,
                            layer_base + "shadows/" + anim + ".csx", reason))
                return Error{ErrorCode::format, reason};

            auto seq_data = vfs.read(set + "seq/" + anim + "_" + layer.category + ".seq");
            if (!seq_data) {
                gl::log(gl::LogLevel::warning,
                        "wear seq missing: " + anim + "_" + layer.category + " (layer skipped)");
                continue;
            }
            auto seq = gl::assets::parse_wear_seq(seq_data.value());
            if (!seq)
                return seq.error();
            if (seq->stride != header.columns || seq->rows != header.rows)
                return Error{ErrorCode::format,
                             "wear seq size mismatch: " + anim + "_" + layer.category};
            dst.seq = std::move(seq.value());
            dst.block = block;
            dst.valid = true;
        }

        std::vector<WearLayerImages> composed;
        composed.reserve(5);
        for (auto &layer : loaded)
            if (layer.valid)
                composed.push_back(
                    WearLayerImages{layer.block, &layer.body, &layer.shadow, &layer.seq});

        out.body = compose_wear_body(header, out.body, composed);
        out.shadow = compose_wear_shadow(header, out.shadow, composed);
        visual.animations_[header.type_mask] = std::move(out);
    }
    if (visual.animations_.find(0x4) == visual.animations_.end())
        return Error{ErrorCode::format, "wear visual has no tb_stay animation"};
    return visual;
}
} // namespace gl::game
