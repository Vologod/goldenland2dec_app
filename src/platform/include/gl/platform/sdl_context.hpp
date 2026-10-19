#pragma once

#include <SDL.h>

namespace gl::platform {

class SdlContext {
public:
    SdlContext();
    ~SdlContext();

    SdlContext(const SdlContext&) = delete;
    SdlContext& operator=(const SdlContext&) = delete;
    SdlContext(SdlContext&&) = delete;
    SdlContext& operator=(SdlContext&&) = delete;
};

class Window {
public:
    Window(const char* title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] SDL_Window* window() const noexcept { return window_; }
    [[nodiscard]] SDL_Renderer* renderer() const noexcept { return renderer_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

}
