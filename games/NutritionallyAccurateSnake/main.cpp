#include "snake.hpp"

#include <svanes/application.hpp>

#include <exception>
#include <iostream>

int main() {
    try {
        nutritionally_accurate_snake::Snake game;
        svanes::Application application(
            {.title = "Nutritionally Accurate Snake",
             .width = 1280,
             .height = 720});
        return application.run(game);
    } catch (const std::exception &error) {
        std::cerr << "Nutritionally Accurate Snake Error: " << error.what()
                  << '\n';
        return 1;
    }
}
