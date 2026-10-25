#include "gl/game/hero_wear.hpp"

#include <doctest/doctest.h>

#include <cstdint>

using namespace gl;
using namespace gl::game;

namespace {
assets::Image make_image(std::uint32_t w, std::uint32_t h, assets::Rgba c) {
    assets::Image img;
    img.width = w;
    img.height = h;
    img.rgba.assign(static_cast<std::size_t>(w) * h, c);
    return img;
}
} // namespace

TEST_CASE("compose_wear_body places a layer crop by engine formula") {
    assets::WearHadAnimation a;
    a.type_mask = 0x4;
    a.rows = 1;
    a.columns = 2;
    a.frame_width = 2;
    a.frame_height = 2;
    a.layer_offset_x = 3;
    a.layer_offset_y = 1;
    auto base = make_image(4, 2, assets::Rgba{1, 0, 0, 255});
    auto layer_img = make_image(4, 2, assets::Rgba{9, 9, 9, 255});
    assets::IadBlock block;
    block.frames = 2;
    block.frame_width = 2;
    block.frame_height = 2;
    block.layer_offset_x = 3;
    block.layer_offset_y = 1;
    block.crops = {{0, 0, 2, 2}, {0, 0, 2, 2}};
    assets::WearSequence seq;
    seq.stride = 2;
    seq.rows = 1;
    seq.order = {2, 2};
    WearLayerImages layer{&block, &layer_img, nullptr, &seq};
    WearLayerImages layers[1] = {layer};
    auto out = compose_wear_body(a, base, layers);
    CHECK(out.width == 4);
    CHECK(out.height == 2);
    CHECK(out.rgba[0].r == 9);
    CHECK(out.rgba[1].r == 9);
    CHECK(out.rgba[2].r == 9);
}

TEST_CASE("compose_wear_body draws order 0/1 layers behind the base") {
    assets::WearHadAnimation a;
    a.rows = 1;
    a.columns = 1;
    a.frame_width = 2;
    a.frame_height = 1;
    auto base = make_image(2, 1, assets::Rgba{1, 0, 0, 255});
    auto layer_img = make_image(2, 1, assets::Rgba{9, 0, 0, 255});
    assets::IadBlock block;
    block.frames = 1;
    block.frame_width = 2;
    block.frame_height = 1;
    block.crops = {{0, 0, 2, 1}};
    assets::WearSequence seq;
    seq.stride = 1;
    seq.rows = 1;
    seq.order = {1};
    WearLayerImages layer{&block, &layer_img, nullptr, &seq};
    WearLayerImages layers[1] = {layer};
    auto out = compose_wear_body(a, base, layers);
    CHECK(out.rgba[0].r == 1);
    CHECK(out.rgba[1].r == 1);
}

TEST_CASE("compose_wear_shadow clips to the sheet") {
    assets::WearHadAnimation a;
    a.rows = 2;
    a.columns = 1;
    a.frame_width = 2;
    a.frame_height = 2;
    a.shadow_width = 2;
    a.shadow_height = 2;
    a.shadow_offset_x = 0;
    a.shadow_offset_y = 0;
    auto base = make_image(2, 4, assets::Rgba{3, 0, 0, 0});
    assets::IadBlock block;
    block.frames = 1;
    block.shadow_frame_width = 2;
    block.shadow_frame_height = 2;
    block.shadow_layer_offset_x = 0;
    block.shadow_layer_offset_y = 0;
    block.shadow_crops = {{0, 0, 2, 2}, {0, 0, 2, 2}};
    auto layer_img = make_image(2, 4, assets::Rgba{7, 0, 0, 255});
    WearLayerImages layer{&block, nullptr, &layer_img, nullptr};
    WearLayerImages layers[1] = {layer};
    auto out = compose_wear_shadow(a, base, layers);
    CHECK(out.width == 2);
    CHECK(out.height == 4);
    CHECK(out.rgba[0].r == 7);
    CHECK(out.rgba[6].r == 7);
}
