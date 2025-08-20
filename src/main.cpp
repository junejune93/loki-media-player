#include "core/Application.h"
#include <iostream>

int main() {
    try {
        Application app;

        if (!app.initialize()) {
            std::cerr << "Failed to initialize application\n";
            return -1;
        }

        app.run();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}