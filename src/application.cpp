// This code was permanently borrowed from the following repository
// https://github.com/AlexanderCard/CSC481-581-M1

#include <svanes/application.hpp>

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/audio/audio_manager.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/game.hpp>
#include <svanes/input.hpp>
#include <svanes/physics_system.hpp>
#include <svanes/render/render_queue.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include "audio/audio_manager_internal.hpp"
#include "input_manager_internal.hpp"
#include "render/render_queue_executor.hpp"
#include "render/texture_manager_internal.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace svanes {

namespace internal {

/**
 * Internal helper function that runs the main game loop.
 * This should not be exposed to the user of the engine.
 * This function shall handle input processing, calculating delta time,
 * updating the game state, and building and executing the render queue for each
 * frame.
 * @param game The game instance to run within the application.
 * @param window The SDL_Window used for input.
 * @param renderer The SDL_Renderer used for rendering.
 * @param world The registry containing all entities and their components.
 */
void RunGameLoop(IGame &game, SDL_Window *window, SDL_Renderer *renderer,
                 Registry &world) {

    // First time setup.

    TextureManager texture_manager = TextureManagerInternal::Create(renderer);
    AudioManager audio_manager = AudioManagerInternal::Create();
    RenderQueueExecutor render_queue_executor{renderer, texture_manager};
    RenderQueue render_queue;
    InputManager input;
    Camera2D camera;
    Vector2D gravity{};
    std::int32_t output_width = 0;
    std::int32_t output_height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer, &output_width,
                                        &output_height)) {
        throw std::runtime_error("Could not get render output dimensions: " +
                                 std::string{SDL_GetError()});
    }
    camera.SetOutputSize(output_width, output_height);
    GameContext game_context{world,  texture_manager, audio_manager,
                             camera, output_width,    output_height,
                             gravity};

    // Custom initialization of the game. Implemented by the user of the engine.

    game.Initialize(game_context);

    // The fixed number of tics in a simulation step. This is used to
    // determine whether a simulation step is due based on accumulated real
    // time. The engine requires this to be positive, and it is copied from the
    // game context after initialization. Each rendered frame runs zero or one
    // steps. Slow frames slow simulated time without changing step size.
    const TicCount physics_step_tics = game_context.physics_step_tics;

    if (physics_step_tics == 0) {
        throw std::invalid_argument("The physics step must be positive.");
    }

    AsyncParallelForDriver parallel_for(game_context.concurrency);

    // fixed origin for measuring elapsed time, so truncation to whole
    // microseconds does not discard part of a microsecond on every frame.
    const auto start_time = std::chrono::steady_clock::now();

    // The previous time in tics, used to calculate the delta time for each
    // render frame.
    TicCount previous_tics = 0;

    // Real tics accumulated toward the next simulation step. After a step,
    // only the fractional step remains. Overdue whole steps are lost.
    TicCount pending_tics = 0;

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

        //
        // DELTA TIME CALCULATION
        //
        const TicCount current_tics = static_cast<TicCount>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start_time)
                .count());

        const TicCount real_delta_tics = current_tics - previous_tics;
        previous_tics = current_tics;

        pending_tics += real_delta_tics;


        //
        // GAME STATE UPDATE
        //

        if (!SDL_GetCurrentRenderOutputSize(renderer, &output_width,
                                            &output_height)) {
            throw std::runtime_error(
                "Could not get render output dimensions: " +
                std::string{SDL_GetError()});
        }
        camera.SetOutputSize(output_width, output_height);

        const FrameContext frame_context{
            world,         input,         real_delta_tics, output_width,
            output_height, audio_manager, camera,          gravity};

        // Here we should advance the kinematics of all entities before each
        // game physics update. This allows us to first update the positions of
        // all entities based on their velocities and accelerations, and then
        // allow the game logic to respond to those new positions. Fixes the
        // broken behavior where the game logic was responding to the previous
        // step's positions, which could lead to incorrect behavior likely
        // around collisions.
        //
        // This does however mean that there is now one physics step of input
        // latency, so we can talk about whether this is the best approach or
        // not.
        //
        // Suppose our simulation step is 10000 source tics, and this frame
        // accumulated 16000 source tics. We advance one complete simulation
        // step and retain the remaining 6000. Advancing a smaller step would
        // make the simulation depend on rendering time. Another 4000 real
        // tics will make the next step due. If we accumulated 35000 instead,
        // we still run only one step and retain 5000, discarding 20000 overdue
        // tics. Keeping the fraction preserves the nominal average rate when
        // rendering is frequent enough. Discarding whole intervals prevents
        // slow frames from scheduling more work and causing even slower frames.
        // One expensive step can still delay rendering on this same thread.
        //
        // Each simulation step advances the shared clock by physics_step_tics.
        // Timelines convert that source time delta through their parents and
        // their own rates. A double speed entity receives 20000 local tics per
        // step, while a half speed entity receives 5000. Both participate in
        // the same number of simulation steps. ONLY their local elapsed time
        // differ. Normal rate timelines follow completed simulation time,
        // not real time. Discarded steps never advance the simulation.
        if (pending_tics >= physics_step_tics) {
            AdvanceTimelines(world, physics_step_tics);
            AdvancePhysics(world, gravity, parallel_for,
                           [&](std::span<const PhysicsTimeStep> steps) {
                               game.PhysicsUpdate({world, input, steps});
                           });

            // Animation uses the delta reported by this timeline pass.
            // Doing this on a frame without a step would reuse the last delta.
            AdvanceSpriteAnimations(world);
            pending_tics %= physics_step_tics;
        }
        game.Update(frame_context);

        InputManagerInternal::SynchronizeTextInput(input, window);

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

        // This will add entities to the rendering queue, but will not actually
        // render them. Actual rendering takes place in the executor.

        SubmitShapes(world, render_queue, camera);
        SubmitSprites(world, render_queue, camera);
        SubmitRadialGradients(world, render_queue, camera);

        render_queue_executor.Execute(
            render_queue,
            // Our clip rectangle is only relevant when we are in proportional
            // scaling mode.
            camera.scale_mode == ScaleMode::Proportional
                ? std::optional{camera.Viewport()}
                : std::nullopt);

        SDL_RenderPresent(renderer);

        // Implicit limit to 1000 FPS to avoid essentially just busy waiting
        // wasting CPU resources.
        SDL_Delay(1);
    }
}

} // namespace internal

Application::Application(ApplicationSettings settings)
    : settings(std::move(settings)) {}

int32_t Application::run(IGame &game) {

    // SDL initialization and window/renderer creation.

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;

    const bool created = SDL_CreateWindowAndRenderer(
        settings.title.c_str(), settings.width, settings.height,
        SDL_WINDOW_RESIZABLE, &window, &renderer);

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
