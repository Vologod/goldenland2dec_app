#include <gl/platform/sdl_context.hpp>

#include <stdexcept>
#include <string>

namespace gl::platform {
namespace {

[[noreturn]] void throw_sdl_error(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + SDL_GetError());
}

} // namespace

SdlContext::SdlContext() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        throw_sdl_error("SDL_Init failed");
    }
}

SdlContext::~SdlContext() { SDL_Quit(); }

Window::Window(const char* title, int width, int height) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                               height, SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        throw_sdl_error("SDL_CreateWindow failed");
    }

    const Uint32 renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    renderer_ = SDL_CreateRenderer(window_, -1, renderer_flags);
    if (renderer_ == nullptr) {
        const std::string error = SDL_GetError();
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        throw std::runtime_error("SDL_CreateRenderer failed: " + error);
    }
}

Window::~Window() {
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
    }
}

} // namespace gl::platform
