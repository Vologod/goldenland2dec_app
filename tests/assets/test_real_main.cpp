#define DOCTEST_CONFIG_IMPLEMENT
#include <cstdlib>
#include <doctest/doctest.h>

int main(int argc, char **argv) {
    doctest::Context context(argc, argv);
    const int result = context.run();
    if (std::getenv("GL_GAME_DIR") == nullptr)
        return 77;
    return result;
}
