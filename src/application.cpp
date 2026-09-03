// This code was permanently borrowed from the following repository
// https://github.com/AlexanderCard/CSC481-581-M1

#include <svanes/application.hpp>

#include <svanes/game.hpp>
#include <svanes/input.hpp>
#include <svanes/render/render_queue.hpp>

#include "render/render_queue_executor.hpp"
#include "render/texture_manager_internal.hpp"

#include <SDL3/SDL.h>

#include <utility>

namespace svanes {

namespace internal {

/**
 * Internal helper function that runs the main game loop. 
 * This should not be exposed to the user of the engine.
 * This function shall handle input processing, calculating delta time,
 * updating the game state, and building and executing the render queue for each frame.
 * @param game The game instance to run within the application.
 * @param renderer The SDL_Renderer used for rendering.
 */
void RunGameLoop(IGame& game, SDL_Renderer* renderer)
{

    // First time setup.

    TextureManager texture_manager = TextureManagerInternal::Create(renderer);
    RenderQueueExecutor render_queue_executor{renderer, texture_manager};
    RenderQueue render_queue;
    InputManager input;
    GameContext game_context{texture_manager};

    // Custom initialization of the game. Implemented by the user of the engine.

    game.Initialize(game_context);

    Uint64 previous_ticks = SDL_GetTicks();

    bool running = true;
    while (running) {

        //
        // INPUT DETECTION AND PROCESSING
        //

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


        //
        // GAME STATE UPDATE
        //

        const FrameContext frame_context{input, delta_seconds};
        game.Update(frame_context);

        if (game.ShouldQuit()) {
            running = false;
        }

        //
        // RENDERING
        //

        int32_t output_width = 0;
        int32_t output_height = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height);

        render_queue.Reset();
        const RenderContext render_context{render_queue, output_width, output_height};
        game.BuildRenderQueue(render_context);
        render_queue_executor.Execute(render_queue);
        SDL_RenderPresent(renderer);

        // Implicit limit to 1000 FPS to avoid essentially just busy waiting
        // wasting CPU resources.
        SDL_Delay(1);
    }
}

}

Application::Application(ApplicationSettings settings)
    : settings(std::move(settings))
{
}

int32_t Application::run(IGame& game) const
{

    // SDL initialization and window/renderer creation.

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    const bool created = SDL_CreateWindowAndRenderer(
        settings.title.c_str(),
        settings.width,
        settings.height,
        SDL_WINDOW_RESIZABLE,
        &window,
        &renderer
    );

    if (!created) {
        SDL_Log("Could not create the window and renderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // This is where the actual logic of the game loop is executed.
    internal::RunGameLoop(game, renderer);

    // Cleanup

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace svanes
