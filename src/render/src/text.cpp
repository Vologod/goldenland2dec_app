#include "gl/render/text.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <stb_easy_font.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace gl::render {
namespace {

constexpr int kQuadsPerCharacter = 16;

std::string sanitize(std::string_view ascii) {
    std::string text;
    text.reserve(ascii.size());
    for (const char ch : ascii) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte == '\n' || (byte >= 0x20 && byte < 0x7f)) {
            text.push_back(ch);
        } else {
            text.push_back('?');
        }
    }
    return text;
}

}

void draw_text(SDL_Renderer* renderer, std::string_view ascii, float x, float y, float scale,
               Color color) {
    if (renderer == nullptr || ascii.empty() || scale <= 0.0f) {
        return;
    }

    std::string text = sanitize(ascii);

    const std::size_t vertex_count =
        (text.size() * static_cast<std::size_t>(kQuadsPerCharacter) + 1) * 4;
    std::vector<std::uint8_t> buffer(vertex_count * 16);
    unsigned char rgba[4] = {color.r, color.g, color.b, color.a};

    const int quads = stb_easy_font_print(0.0f, 0.0f, text.data(), rgba, buffer.data(),
                                          static_cast<int>(buffer.size()));
    if (quads <= 0) {
        return;
    }

    std::vector<SDL_Vertex> vertices(static_cast<std::size_t>(quads) * 4);
    std::vector<int> indices(static_cast<std::size_t>(quads) * 6);

    for (int quad = 0; quad < quads; ++quad) {
        for (int corner = 0; corner < 4; ++corner) {
            const std::uint8_t* raw =
                buffer.data() + (static_cast<std::size_t>(quad) * 4 + corner) * 16;
            float vertex_x = 0.0f;
            float vertex_y = 0.0f;
            std::memcpy(&vertex_x, raw, sizeof(float));
            std::memcpy(&vertex_y, raw + sizeof(float), sizeof(float));

            SDL_Vertex& vertex =
                vertices[static_cast<std::size_t>(quad) * 4 + static_cast<std::size_t>(corner)];
            vertex.position.x = x + vertex_x * scale;
            vertex.position.y = y + vertex_y * scale;
            vertex.color = SDL_Color{color.r, color.g, color.b, color.a};
            vertex.tex_coord.x = 0.0f;
            vertex.tex_coord.y = 0.0f;
        }

        const int base = quad * 4;
        int* index = indices.data() + static_cast<std::size_t>(quad) * 6;
        index[0] = base;
        index[1] = base + 1;
        index[2] = base + 2;
        index[3] = base;
        index[4] = base + 2;
        index[5] = base + 3;
    }

    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

float text_width(std::string_view ascii, float scale) {
    if (ascii.empty() || scale <= 0.0f) {
        return 0.0f;
    }
    std::string text = sanitize(ascii);
    return static_cast<float>(stb_easy_font_width(text.data())) * scale;
}

float text_height(float scale) {
    if (scale <= 0.0f) {
        return 0.0f;
    }
    char text[] = "A";
    return static_cast<float>(stb_easy_font_height(text)) * scale;
}

}
