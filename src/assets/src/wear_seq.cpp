#include "gl/assets/wear_seq.hpp"
#include <string>

namespace gl::assets {
namespace {
Error format_error(std::string message) { return Error{ErrorCode::format, std::move(message)}; }
} // namespace

Result<WearSequence> parse_wear_seq(std::span<const std::uint8_t> data) {
    if (data.empty())
        return format_error("wear seq is empty");
    WearSequence seq;
    seq.stride = data[0];
    if (seq.stride == 0)
        return format_error("wear seq stride is zero");
    if ((data.size() - 1) % seq.stride != 0)
        return format_error("wear seq size is not a multiple of stride");
    seq.rows = static_cast<std::uint32_t>((data.size() - 1) / seq.stride);
    seq.order.assign(data.begin() + 1, data.end());
    for (std::uint8_t value : seq.order)
        if (value > 4)
            return format_error("wear seq draw order exceeds 4");
    return seq;
}
} // namespace gl::assets
