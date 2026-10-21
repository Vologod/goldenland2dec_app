#include "gl/render/font.hpp"

#include "gl/render/utf8.hpp"

#include <stb_truetype.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gl::render {
namespace {

constexpr char32_t kFallbackCodepoint = U'?';
constexpr int kAtlasWidth = 512;
constexpr int kPadding = 2;
constexpr float kMinPixelHeight = 8.0f;
constexpr float kMaxPixelHeight = 256.0f;
constexpr int kTabSpaces = 4;

struct Glyph {
    float advance = 0.0f;
    float xoff = 0.0f;
    float yoff = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
    bool visible = false;
};

struct PendingGlyph {
    char32_t codepoint = 0;
    int width = 0;
    int height = 0;
    int xoff = 0;
    int yoff = 0;
    int x = 0;
    int y = 0;
    std::vector<unsigned char> bitmap;
};

std::vector<char32_t> normalize_codepoints(std::string_view utf8) {
    std::vector<char32_t> codepoints;
    for (const char32_t codepoint : utf8_decode(utf8)) {
        if (codepoint == U'\t') {
            codepoints.insert(codepoints.end(), kTabSpaces, U' ');
            continue;
        }
        if (codepoint == U'\r' || (codepoint < 0x20u && codepoint != U'\n') ||
            (codepoint >= 0x7Fu && codepoint <= 0x9Fu)) {
            continue;
        }
        codepoints.push_back(codepoint);
    }
    return codepoints;
}

std::string encode_utf8(std::span<const char32_t> codepoints) {
    std::string out;
    out.reserve(codepoints.size());
    for (const char32_t codepoint : codepoints) {
        if (codepoint <= 0x7Fu) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FFu) {
            out.push_back(static_cast<char>(0xC0u | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        } else if (codepoint <= 0xFFFFu) {
            out.push_back(static_cast<char>(0xE0u | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        } else {
            out.push_back(static_cast<char>(0xF0u | (codepoint >> 18)));
            out.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
    }
    return out;
}

} // namespace

struct Font::Impl {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    float scale = 1.0f;
    float ascent = 0.0f;
    float line_height = 0.0f;
    SDL_Texture* texture = nullptr;
    std::unordered_map<char32_t, Glyph> glyphs;

    ~Impl() {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }

    const Glyph* glyph_for(char32_t codepoint) const {
        const auto found = glyphs.find(codepoint);
        if (found != glyphs.end()) {
            return &found->second;
        }
        const auto fallback = glyphs.find(kFallbackCodepoint);
        if (fallback != glyphs.end()) {
            return &fallback->second;
        }
        return nullptr;
    }

    float kerning(char32_t previous, char32_t current) const {
        if (previous == 0) {
            return 0.0f;
        }
        return static_cast<float>(stbtt_GetCodepointKernAdvance(&info, static_cast<int>(previous),
                                                                static_cast<int>(current))) *
               scale;
    }
};

namespace {

float line_width(const Font::Impl& impl, std::span<const char32_t> codepoints) {
    float width = 0.0f;
    char32_t previous = 0;
    for (const char32_t codepoint : codepoints) {
        width += impl.kerning(previous, codepoint);
        const Glyph* glyph = impl.glyph_for(codepoint);
        if (glyph != nullptr) {
            width += glyph->advance;
        }
        previous = codepoint;
    }
    return width;
}

void emit_line(std::vector<std::string>& lines, std::span<const char32_t> codepoints) {
    std::size_t end = codepoints.size();
    while (end > 0 && codepoints[end - 1] == U' ') {
        --end;
    }
    lines.push_back(encode_utf8(codepoints.first(end)));
}

} // namespace

Font::Font(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Font::Font(Font&& other) noexcept = default;

Font& Font::operator=(Font&& other) noexcept = default;

Font::~Font() = default;

Result<Font> Font::load_from_file(SDL_Renderer* renderer, const std::filesystem::path& path,
                                  float pixel_height) {
    if (renderer == nullptr) {
        return Error{ErrorCode::argument, "font: renderer is null"};
    }
    if (pixel_height < kMinPixelHeight || pixel_height > kMaxPixelHeight) {
        return Error{ErrorCode::argument, "font: pixel height " + std::to_string(pixel_height) +
                                              " outside supported range [8, 256]"};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return Error{ErrorCode::io, "font: cannot open " + path.string()};
    }

    auto impl = std::make_unique<Impl>();
    impl->data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (impl->data.empty()) {
        return Error{ErrorCode::format, "font: empty file " + path.string()};
    }

    const int offset = stbtt_GetFontOffsetForIndex(impl->data.data(), 0);
    if (offset < 0 || stbtt_InitFont(&impl->info, impl->data.data(), offset) == 0) {
        return Error{ErrorCode::format, "font: invalid TrueType data " + path.string()};
    }

    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&impl->info, &ascent, &descent, &line_gap);
    impl->scale = stbtt_ScaleForPixelHeight(&impl->info, pixel_height);
    impl->ascent = static_cast<float>(ascent) * impl->scale;
    impl->line_height = static_cast<float>(ascent - descent + line_gap) * impl->scale;

    static constexpr std::array<std::pair<char32_t, char32_t>, 3> kRanges = {
        {{0x20u, 0x7Eu}, {0x0400u, 0x04FFu}, {0x2000u, 0x206Fu}}};

    std::vector<PendingGlyph> pending;
    for (const auto& [first, last] : kRanges) {
        for (char32_t codepoint = first; codepoint <= last; ++codepoint) {
            if (stbtt_FindGlyphIndex(&impl->info, static_cast<int>(codepoint)) == 0) {
                continue;
            }

            int advance = 0;
            int left_side_bearing = 0;
            stbtt_GetCodepointHMetrics(&impl->info, static_cast<int>(codepoint), &advance,
                                       &left_side_bearing);

            Glyph glyph;
            glyph.advance = static_cast<float>(advance) * impl->scale;

            int width = 0;
            int height = 0;
            int xoff = 0;
            int yoff = 0;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&impl->info, 0.0f, impl->scale,
                                                             static_cast<int>(codepoint), &width,
                                                             &height, &xoff, &yoff);

            if (bitmap != nullptr && width > 0 && height > 0) {
                PendingGlyph glyph_bitmap;
                glyph_bitmap.codepoint = codepoint;
                glyph_bitmap.width = width;
                glyph_bitmap.height = height;
                glyph_bitmap.xoff = xoff;
                glyph_bitmap.yoff = yoff;
                glyph_bitmap.bitmap.assign(bitmap, bitmap + static_cast<std::size_t>(width) *
                                                                static_cast<std::size_t>(height));
                pending.push_back(std::move(glyph_bitmap));

                glyph.xoff = static_cast<float>(xoff);
                glyph.yoff = static_cast<float>(yoff);
                glyph.width = static_cast<float>(width);
                glyph.height = static_cast<float>(height);
                glyph.visible = true;
            }
            stbtt_FreeBitmap(bitmap, nullptr);

            impl->glyphs.emplace(codepoint, glyph);
        }
    }

    int atlas_width = kAtlasWidth;
    for (const PendingGlyph& glyph : pending) {
        atlas_width = std::max(atlas_width, glyph.width + 2 * kPadding);
    }

    int pen_x = kPadding;
    int pen_y = kPadding;
    int row_height = 0;
    int atlas_height = kPadding;
    for (PendingGlyph& glyph : pending) {
        if (glyph.width + 2 * kPadding > atlas_width) {
            return Error{ErrorCode::format, "font: glyph exceeds atlas row width"};
        }
        if (pen_x + glyph.width + kPadding > atlas_width) {
            pen_x = kPadding;
            pen_y += row_height + kPadding;
            row_height = 0;
        }
        glyph.x = pen_x;
        glyph.y = pen_y;
        pen_x += glyph.width + kPadding;
        row_height = std::max(row_height, glyph.height);
        atlas_height = std::max(atlas_height, pen_y + glyph.height);
    }
    atlas_height += kPadding;
    if (atlas_height < 1) {
        atlas_height = 1;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(atlas_width) * static_cast<std::size_t>(atlas_height);
    std::vector<unsigned char> pixels(pixel_count * 4, 0);
    for (std::size_t index = 0; index < pixel_count; ++index) {
        pixels[index * 4 + 0] = 255;
        pixels[index * 4 + 1] = 255;
        pixels[index * 4 + 2] = 255;
    }

    for (const PendingGlyph& glyph : pending) {
        for (int row = 0; row < glyph.height; ++row) {
            const std::size_t destination =
                (static_cast<std::size_t>(glyph.y + row) * static_cast<std::size_t>(atlas_width) +
                 static_cast<std::size_t>(glyph.x)) *
                4;
            for (int column = 0; column < glyph.width; ++column) {
                pixels[destination + static_cast<std::size_t>(column) * 4 + 3] =
                    glyph.bitmap[static_cast<std::size_t>(row) *
                                     static_cast<std::size_t>(glyph.width) +
                                 static_cast<std::size_t>(column)];
            }
        }

        auto found = impl->glyphs.find(glyph.codepoint);
        if (found == impl->glyphs.end()) {
            continue;
        }
        Glyph& target = found->second;
        const float atlas_width_f = static_cast<float>(atlas_width);
        const float atlas_height_f = static_cast<float>(atlas_height);
        target.u0 = (static_cast<float>(glyph.x) + 0.5f) / atlas_width_f;
        target.v0 = (static_cast<float>(glyph.y) + 0.5f) / atlas_height_f;
        target.u1 = (static_cast<float>(glyph.x + glyph.width) - 0.5f) / atlas_width_f;
        target.v1 = (static_cast<float>(glyph.y + glyph.height) - 0.5f) / atlas_height_f;
    }

    impl->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                      atlas_width, atlas_height);
    if (impl->texture == nullptr) {
        return Error{ErrorCode::io,
                     std::string("font: texture creation failed: ") + SDL_GetError()};
    }
    if (SDL_UpdateTexture(impl->texture, nullptr, pixels.data(), atlas_width * 4) != 0) {
        return Error{ErrorCode::io, std::string("font: texture upload failed: ") + SDL_GetError()};
    }
    SDL_SetTextureBlendMode(impl->texture, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(impl->texture, SDL_ScaleModeLinear);
#endif

    return Font(std::move(impl));
}

void Font::draw_text(SDL_Renderer* renderer, std::string_view utf8, float x, float y,
                     Color color) const {
    if (renderer == nullptr || impl_ == nullptr || impl_->texture == nullptr || utf8.empty()) {
        return;
    }

    const std::vector<char32_t> codepoints = normalize_codepoints(utf8);
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    vertices.reserve(codepoints.size() * 4);
    indices.reserve(codepoints.size() * 6);

    float pen_x = x;
    float baseline = y + impl_->ascent;
    char32_t previous = 0;

    for (const char32_t codepoint : codepoints) {
        if (codepoint == U'\n') {
            pen_x = x;
            baseline += impl_->line_height;
            previous = 0;
            continue;
        }

        pen_x += impl_->kerning(previous, codepoint);
        const Glyph* glyph = impl_->glyph_for(codepoint);
        if (glyph != nullptr) {
            if (glyph->visible) {
                const float left = pen_x + glyph->xoff;
                const float top = baseline + glyph->yoff;
                const float right = left + glyph->width;
                const float bottom = top + glyph->height;

                const int base = static_cast<int>(vertices.size());
                const SDL_Color vertex_color{color.r, color.g, color.b, color.a};
                vertices.push_back(SDL_Vertex{{left, top}, vertex_color, {glyph->u0, glyph->v0}});
                vertices.push_back(SDL_Vertex{{right, top}, vertex_color, {glyph->u1, glyph->v0}});
                vertices.push_back(
                    SDL_Vertex{{right, bottom}, vertex_color, {glyph->u1, glyph->v1}});
                vertices.push_back(
                    SDL_Vertex{{left, bottom}, vertex_color, {glyph->u0, glyph->v1}});
                indices.push_back(base);
                indices.push_back(base + 1);
                indices.push_back(base + 2);
                indices.push_back(base);
                indices.push_back(base + 2);
                indices.push_back(base + 3);
            }
            pen_x += glyph->advance;
        }
        previous = codepoint;
    }

    if (vertices.empty()) {
        return;
    }

    SDL_RenderGeometry(renderer, impl_->texture, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

Vec2 Font::measure(std::string_view utf8) const {
    if (impl_ == nullptr || utf8.empty()) {
        return Vec2{};
    }

    const std::vector<char32_t> codepoints = normalize_codepoints(utf8);
    if (codepoints.empty()) {
        return Vec2{};
    }

    float width = 0.0f;
    float widest = 0.0f;
    float height = impl_->line_height;
    char32_t previous = 0;

    for (const char32_t codepoint : codepoints) {
        if (codepoint == U'\n') {
            widest = std::max(widest, width);
            width = 0.0f;
            previous = 0;
            height += impl_->line_height;
            continue;
        }
        width += impl_->kerning(previous, codepoint);
        const Glyph* glyph = impl_->glyph_for(codepoint);
        if (glyph != nullptr) {
            width += glyph->advance;
        }
        previous = codepoint;
    }

    widest = std::max(widest, width);
    return Vec2{widest, height};
}

float Font::line_height() const { return impl_ != nullptr ? impl_->line_height : 0.0f; }

std::vector<std::string> Font::wrap(std::string_view utf8, float max_width) const {
    std::vector<std::string> lines;
    if (impl_ == nullptr || utf8.empty()) {
        return lines;
    }

    const std::vector<char32_t> codepoints = normalize_codepoints(utf8);
    if (codepoints.empty()) {
        return lines;
    }

    auto wrap_paragraph = [&](std::span<const char32_t> paragraph) {
        const std::size_t first_line = lines.size();
        if (max_width <= 0.0f) {
            emit_line(lines, paragraph);
            return;
        }
        if (paragraph.empty()) {
            lines.emplace_back();
            return;
        }

        std::vector<char32_t> line;
        std::size_t index = 0;
        while (index < paragraph.size()) {
            while (index < paragraph.size() && paragraph[index] == U' ') {
                ++index;
            }
            if (index >= paragraph.size()) {
                break;
            }
            const std::size_t word_start = index;
            while (index < paragraph.size() && paragraph[index] != U' ') {
                ++index;
            }
            const std::span<const char32_t> word =
                paragraph.subspan(word_start, index - word_start);

            std::vector<char32_t> candidate = line;
            if (!candidate.empty()) {
                candidate.push_back(U' ');
            }
            candidate.insert(candidate.end(), word.begin(), word.end());

            if (line_width(*impl_, candidate) <= max_width) {
                line = std::move(candidate);
                continue;
            }

            if (!line.empty()) {
                emit_line(lines, line);
                line.clear();
            }

            if (line_width(*impl_, word) <= max_width) {
                line.assign(word.begin(), word.end());
                continue;
            }

            std::vector<char32_t> chunk;
            for (const char32_t codepoint : word) {
                chunk.push_back(codepoint);
                if (line_width(*impl_, chunk) > max_width && chunk.size() > 1) {
                    chunk.pop_back();
                    emit_line(lines, chunk);
                    chunk.clear();
                    chunk.push_back(codepoint);
                }
            }
            line = std::move(chunk);
        }

        if (!line.empty()) {
            emit_line(lines, line);
        }
        if (lines.size() == first_line) {
            lines.emplace_back();
        }
    };

    std::size_t paragraph_start = 0;
    while (paragraph_start <= codepoints.size()) {
        std::size_t paragraph_end = paragraph_start;
        while (paragraph_end < codepoints.size() && codepoints[paragraph_end] != U'\n') {
            ++paragraph_end;
        }
        wrap_paragraph(std::span<const char32_t>(codepoints)
                           .subspan(paragraph_start, paragraph_end - paragraph_start));
        if (paragraph_end >= codepoints.size()) {
            break;
        }
        paragraph_start = paragraph_end + 1;
    }

    return lines;
}

std::optional<std::filesystem::path> find_system_font() {
#if defined(_WIN32)
    std::vector<std::filesystem::path> directories;
    directories.emplace_back("C:\\Windows\\Fonts");
    if (const char* windir = std::getenv("WINDIR"); windir != nullptr) {
        directories.emplace_back(std::filesystem::path(windir) / "Fonts");
    }

    static constexpr std::array<const char*, 3> kNames = {"segoeui.ttf", "arial.ttf",
                                                          "consola.ttf"};
    for (const char* name : kNames) {
        for (const std::filesystem::path& directory : directories) {
            std::error_code error;
            const std::filesystem::path candidate = directory / name;
            if (std::filesystem::exists(candidate, error)) {
                return candidate;
            }
        }
    }
#else
    (void)0;
#endif
    return std::nullopt;
}

}
