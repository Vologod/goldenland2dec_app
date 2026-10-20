#include "gl/assets/image.hpp"
#include "gl/platform/sdl_context.hpp"
#include "gl/render/texture.hpp"

#include <SDL.h>
#include <doctest.h>

#include <cstdlib>

namespace {

void use_dummy_video() {
#ifdef _WIN32
    _putenv_s("SDL_VIDEODRIVER", "dummy");
    _putenv_s("SDL_RENDER_DRIVER", "software");
#else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_RENDER_DRIVER", "software", 1);
#endif
}

}

TEST_CASE("Texture::from_image enables alpha blending") {
    use_dummy_video();
    gl::platform::SdlContext context;
    gl::platform::Window window("blend mode test", 64, 64);

    gl::assets::Image image;
    image.width = 2;
    image.height = 1;
    image.rgba = {{255, 0, 0, 255}, {0, 255, 0, 0}};

    auto texture = gl::render::Texture::from_image(window.renderer(), image);
    REQUIRE(texture.has_value());

    SDL_BlendMode mode = SDL_BLENDMODE_NONE;
    REQUIRE(SDL_GetTextureBlendMode(texture->get(), &mode) == 0);
    CHECK(mode == SDL_BLENDMODE_BLEND);
}
