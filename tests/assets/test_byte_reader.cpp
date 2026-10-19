#include <doctest/doctest.h>
#include <limits>
#include "gl/assets/byte_reader.hpp"

using gl::assets::ByteReader;

TEST_CASE("ByteReader reads little-endian integers") {
    const std::uint8_t data[] = {1,2,3,4,5,6,7,8};
    ByteReader r(data);
    std::uint32_t v = 0;
    REQUIRE(r.read_u32le(v));
    CHECK(v == 0x04030201u);
    CHECK(r.position() == 4);

    ByteReader whole(data);
    std::uint64_t q = 0;
    REQUIRE(whole.read_u64le(q));
    CHECK(q == 0x0807060504030201ull);
    CHECK(whole.at_end());
}

TEST_CASE("ByteReader fails without advancing on short input") {
    const std::uint8_t data[] = {1,2,3};
    ByteReader r(data);
    std::uint32_t v = 0;
    CHECK_FALSE(r.read_u32le(v));
    CHECK(r.position() == 0);
}

TEST_CASE("ByteReader reads byte spans") {
    const std::uint8_t data[] = {'a','b','c'};
    ByteReader r(data);
    std::span<const std::uint8_t> out;
    REQUIRE(r.read_bytes(2, out));
    CHECK(out.size() == 2);
    CHECK(out[0] == 'a');
    CHECK(r.remaining().size() == 1);
}

TEST_CASE("ByteReader rejects huge lengths without wrapping") {
    const std::uint8_t data[] = {1,2,3,4,5,6,7,8};
    ByteReader r(data);
    std::uint32_t v = 0;
    REQUIRE(r.read_u32le(v));
    const std::size_t huge = std::numeric_limits<std::size_t>::max();
    std::span<const std::uint8_t> out;
    REQUIRE_FALSE(r.read_bytes(huge, out));
    REQUIRE(r.position() == 4);
    REQUIRE_FALSE(r.skip(huge));
    REQUIRE(r.position() == 4);
}
