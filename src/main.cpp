#include "Application.hpp"

#include <iostream>
#include <stdexcept>
#include <cstdlib>

int main() {
    try {
        Application app;
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
