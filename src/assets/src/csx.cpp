#include "gl/assets/csx.hpp"
#include "gl/assets/byte_reader.hpp"

namespace gl::assets {
Result<CsxImage> decode_csx(std::span<const std::uint8_t> data) {
    ByteReader r(data);
    std::uint32_t color_count = 0, transparent_bgr = 0;
    if (!r.read_u32le(color_count) || !r.read_u32le(transparent_bgr))
        return Error{ErrorCode::format, "truncated CSX header"};
    if (color_count == 0 || color_count > 256)
        return Error{ErrorCode::format, "bad CSX palette size"};
    CsxImage image;
    image.palette_bgr.resize(color_count);
    for (auto& c : image.palette_bgr)
        if (!r.read_u32le(c)) return Error{ErrorCode::format, "truncated CSX palette"};
    if (!r.read_u32le(image.width) || !r.read_u32le(image.height))
        return Error{ErrorCode::format, "truncated CSX dimensions"};
    if (image.width == 0 || image.height == 0 || image.width > 16384 || image.height > 16384)
        return Error{ErrorCode::format, "bad CSX dimensions"};
    std::vector<std::uint32_t> offsets(image.height + 1);
    for (auto& o : offsets)
        if (!r.read_u32le(o)) return Error{ErrorCode::format, "truncated CSX row offsets"};
    const auto rows = r.remaining();

    image.transparent_index = transparent_bgr & 0xFF;
    for (int i = static_cast<int>(color_count) - 1; i >= 0; --i) {
        if (image.palette_bgr[static_cast<std::size_t>(i)] == transparent_bgr) {
            image.transparent_index = static_cast<std::uint32_t>(i);
            break;
        }
    }

    image.indices.assign(static_cast<std::size_t>(image.width) * image.height, 0);
    for (std::uint32_t y = 0; y < image.height; ++y) {
        if (offsets[y] > offsets[y + 1] || offsets[y + 1] > rows.size())
            return Error{ErrorCode::format, "bad CSX row offsets"};
        std::size_t pos = offsets[y];
        const std::size_t end = offsets[y + 1];
        std::uint32_t x = 0;
        while (pos < end) {
            const std::uint8_t code = rows[pos++];
            std::uint32_t color = 0, count = 1;
            switch (code) {
                case 0x69: color = image.transparent_index; break;
                case 0x6A:
                    if (pos + 2 > end) return Error{ErrorCode::format, "truncated CSX run"};
                    color = rows[pos++]; count = rows[pos++]; break;
                case 0x6B:
                    if (pos + 1 > end) return Error{ErrorCode::format, "truncated CSX literal"};
                    color = rows[pos++]; break;
                case 0x6C:
                    if (pos + 1 > end) return Error{ErrorCode::format, "truncated CSX transparent run"};
                    color = image.transparent_index; count = rows[pos++]; break;
                default: color = code; break;
            }
            if (color >= color_count || count == 0 || x + count > image.width)
                return Error{ErrorCode::format, "invalid CSX run"};
            for (std::uint32_t i = 0; i < count; ++i)
                image.indices[static_cast<std::size_t>(y) * image.width + x + i] =
                    static_cast<std::uint8_t>(color);
            x += count;
        }
        if (x != image.width) return Error{ErrorCode::format, "CSX row has wrong width"};
    }

    image.rgba.resize(image.indices.size());
    for (std::size_t i = 0; i < image.indices.size(); ++i) {
        const std::uint8_t index = image.indices[i];
        const std::uint32_t bgr = image.palette_bgr[index];
        image.rgba[i] = Rgba{static_cast<std::uint8_t>((bgr >> 16) & 0xFF),
                             static_cast<std::uint8_t>((bgr >> 8) & 0xFF),
                             static_cast<std::uint8_t>(bgr & 0xFF),
                             index == image.transparent_index ? std::uint8_t{0} : std::uint8_t{255}};
    }
    return image;
}
}
