// This code was permanently borrowed from the following repository
// https://github.com/AlexanderCard/CSC481-581-M1

#include <svanes/application.hpp>

#include <svanes/camera2d.hpp>
#include <svanes/game.hpp>
#include <svanes/input.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/render/render_queue.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <svanes/vector2d.hpp>

#include "input_manager_internal.hpp"
#include "render/render_queue_executor.hpp"
#include "render/texture_manager_internal.hpp"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace svanes {

namespace internal {

/**
 * Internal helper function that runs the main game loop. 
 * This should not be exposed to the user of the engine.
 * This function shall handle input processing, calculating delta time,
 * updating the game state, and building and executing the render queue for each frame.
 * @param game The game instance to run within the application.
 * @param window The SDL_Window used for input.
 * @param renderer The SDL_Renderer used for rendering.
 * @param world The registry containing all entities and their components.
 */
void RunGameLoop(IGame& game, SDL_Window* window, SDL_Renderer* renderer, Registry& world)
{

    // First time setup.

    TextureManager texture_manager = TextureManagerInternal::Create(renderer);
    RenderQueueExecutor render_queue_executor{renderer, texture_manager};
    RenderQueue render_queue;
    InputManager input;
    Camera2D camera;
    ScaleMode scale_mode = ScaleMode::Constant;
    Vector2D gravity{};
    std::int32_t output_width = 0;
    std::int32_t output_height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height)) {
        throw std::runtime_error("Could not get render output dimensions: " + std::string{SDL_GetError()});
    }
    GameContext game_context{world, texture_manager, camera, output_width, output_height, scale_mode, gravity};

    // Custom initialization of the game. Implemented by the user of the engine.

    game.Initialize(game_context);

    Uint64 previous_ticks = SDL_GetTicks();

    bool running = true;
    while (running) {

        //
        // INPUT DETECTION AND PROCESSING
        //

        InputManagerInternal::BeginFrame(input);

        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            InputManagerInternal::HandleEvent(input, event);
        }

        const Uint64 current_ticks = SDL_GetTicks();
        const float delta_seconds = static_cast<float>(current_ticks - previous_ticks) / 1000.0F;
        previous_ticks = current_ticks;


        //
        // GAME STATE UPDATE
        //

        if (!SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height)) {
            throw std::runtime_error("Could not get render output dimensions: " + std::string{SDL_GetError()});
        }

        const FrameContext frame_context{world, input, delta_seconds, output_width, output_height, camera, scale_mode, gravity};

        // Here we should advance the kinematics of all entities before updating the game state.
		// This allows us to first update the positions of all entities based on their velocities 
        // and accelerations, and then allow the game logic to respond to those new positions.
		// Fixes the broken behavior where the game logic was responding to the previous frame's positions,
		// which could lead to incorrect behavior likely around collisions.
        //
        // This does however mean that there is now one frame of input latency, so we can talk about
        // whether this is the best approach or not.
        AdvanceKinematics(world, delta_seconds, gravity);
        game.Update(frame_context);
        
        InputManagerInternal::SynchronizeTextInput(input, window);
        AdvanceSpriteAnimations(world, delta_seconds);

        if (game.ShouldQuit()) {
            running = false;
        }

        //
        // RENDERING
        //

        render_queue.Reset();

        // Clear the screen to black before submitting any
        // rendering commands to the render queue.
        // Note that we may want to change this in the future to allow
        // games to retain the previous frame's rendering. 

        render_queue.Clear(Color{});

        SubmitShapes(world, render_queue, camera, output_width, output_height, scale_mode);
        SubmitSprites(world, render_queue, camera, output_width, output_height, scale_mode);
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

int32_t Application::run(IGame& game)
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
    internal::RunGameLoop(game, window, renderer, world);

    // Cleanup

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace svanes
