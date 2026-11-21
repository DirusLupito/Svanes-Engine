#include "grindset.hpp"

#include <svanes/application.hpp>

#include <exception>
#include <iostream>

int main() {
    try {
        grindset::Grindset game;
        svanes::Application application(
            {.title = "Grindset", .width = 1920, .height = 1080});
        return application.run(game);
    } catch (const std::exception &error) {
        std::cerr << "Grindset Error: " << error.what() << '\n';
        return 1;
    }
}
