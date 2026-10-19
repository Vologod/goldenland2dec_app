#include "gl/assets/byte_reader.hpp"
namespace gl::assets {
bool ByteReader::read_u8(std::uint8_t& out) {
    if (pos_ + 1 > data_.size()) return false;
    out = data_[pos_++];
    return true;
}
bool ByteReader::read_u16le(std::uint16_t& out) {
    if (pos_ + 2 > data_.size()) return false;
    out = static_cast<std::uint16_t>(data_[pos_]) |
          static_cast<std::uint16_t>(data_[pos_ + 1] << 8);
    pos_ += 2;
    return true;
}
bool ByteReader::read_u32le(std::uint32_t& out) {
    if (pos_ + 4 > data_.size()) return false;
    out = static_cast<std::uint32_t>(data_[pos_]) |
          (static_cast<std::uint32_t>(data_[pos_ + 1]) << 8) |
          (static_cast<std::uint32_t>(data_[pos_ + 2]) << 16) |
          (static_cast<std::uint32_t>(data_[pos_ + 3]) << 24);
    pos_ += 4;
    return true;
}
bool ByteReader::read_u64le(std::uint64_t& out) {
    if (pos_ + 8 > data_.size()) return false;
    out = 0;
    for (int i = 7; i >= 0; --i) out = (out << 8) | data_[pos_ + i];
    pos_ += 8;
    return true;
}
bool ByteReader::read_bytes(std::size_t n, std::span<const std::uint8_t>& out) {
    if (pos_ + n > data_.size()) return false;
    out = data_.subspan(pos_, n);
    pos_ += n;
    return true;
}
bool ByteReader::skip(std::size_t n) {
    if (pos_ + n > data_.size()) return false;
    pos_ += n;
    return true;
}
}
