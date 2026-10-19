#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace gl::assets {
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}
    bool read_u8(std::uint8_t& out);
    bool read_u16le(std::uint16_t& out);
    bool read_u32le(std::uint32_t& out);
    bool read_u64le(std::uint64_t& out);
    bool read_bytes(std::size_t n, std::span<const std::uint8_t>& out);
    bool skip(std::size_t n);
    std::span<const std::uint8_t> remaining() const { return data_.subspan(pos_); }
    std::size_t position() const { return pos_; }
    bool at_end() const { return pos_ >= data_.size(); }
private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};
}
