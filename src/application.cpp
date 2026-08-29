// This code was permanently borrowed from the following repository
// https://github.com/AlexanderCard/CSC481-581-M1

#include <svanes/application.hpp>

#include <svanes/game.hpp>
#include <svanes/input.hpp>

#include <SDL3/SDL.h>

#include <utility>

namespace svanes {

Application::Application(ApplicationSettings settings)
    : settings_(std::move(settings))
{
}

int Application::run(IGame& game) const
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

    Uint64 previous_ticks = SDL_GetTicks();

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

        const Uint64 current_ticks = SDL_GetTicks();
        const float delta_seconds = static_cast<float>(current_ticks - previous_ticks) / 1000.0F;
        previous_ticks = current_ticks;

        game.OnUpdate(delta_seconds, input);

        if (game.ShouldQuit()) {
            running = false;
        }

        int output_width = 0;
        int output_height = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height);

        game.OnRender(renderer, output_width, output_height);
        SDL_RenderPresent(renderer);

        SDL_Delay(1);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace svanes
