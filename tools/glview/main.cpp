#include <gl/platform/sdl_context.hpp>

#include <cstdio>
#include <exception>
#include <string_view>

namespace {

void print_usage() {
    std::puts("Usage: glview [-h | --help]\n"
              "\n"
              "Options:\n"
              "  -h, --help  Show this help and exit.");
}

}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }
        std::fprintf(stderr, "glview: unknown argument: %s\n", argv[i]);
        print_usage();
        return 1;
    }

    try {
        gl::platform::SdlContext context;
        gl::platform::Window window("Goldenland 2 - Level Viewer", 1280, 720);

        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event) != 0) {
                if (event.type == SDL_QUIT) {
                    running = false;
                } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }

            SDL_SetRenderDrawColor(window.renderer(), 0, 0, 0, 255);
            SDL_RenderClear(window.renderer());
            SDL_RenderPresent(window.renderer());
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glview: %s\n", e.what());
        return 1;
    }

    return 0;
}
