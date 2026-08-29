// This code was permanently borrowed from the following repository
// https://github.com/AlexanderCard/CSC481-581-M1

#include <svanes/application.hpp>

#include <svanes/input.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace svanes {

Application::Application(ApplicationSettings settings)
    : settings_(std::move(settings))
{
}

int Application::run() const
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    const bool created = SDL_CreateWindowAndRenderer(
        settings_.title.c_str(),
        settings_.width,
        settings_.height,
        SDL_WINDOW_RESIZABLE,
        &window,
        &renderer
    );

    if (!created) {
        SDL_Log("Could not create the window and renderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    InputManager input;

    bool running = true;
    while (running) {
        input.BeginFrame();

        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            input.HandleEvent(event);
        }

        int output_width = 0;
        int output_height = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height);

        constexpr float square_size = 96.0F;
        const float available_width = std::max(0.0F, static_cast<float>(output_width) - square_size);
        const float elapsed_seconds = static_cast<float>(SDL_GetTicks()) / 1000.0F;
        const float normalized_position = (std::sin(elapsed_seconds * 2.0F) + 1.0F) * 0.5F;
        const SDL_FRect square{
            normalized_position * available_width,
            (static_cast<float>(output_height) - square_size) * 0.5F,
            square_size,
            square_size,
        };

        SDL_SetRenderDrawColor(renderer, 17, 24, 39, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 56, 189, 248, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &square);
        SDL_RenderPresent(renderer);

        SDL_Delay(1);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace svanes
