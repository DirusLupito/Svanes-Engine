#include <svanes/network/clientserver/network_client.hpp>

#include <svanes/audio/audio_manager.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/network/clientserver/IClient.hpp>
#include <svanes/render/render_queue.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/tilemaps/tilemap.hpp>

#include "../../MenuUtilities/font_manager_internal.hpp"
#include "../../MenuUtilities/text_label_renderer.hpp"
#include "../../audio/audio_manager_internal.hpp"
#include "../../input_manager_internal.hpp"
#include "../../render/render_queue_executor.hpp"
#include "../../render/texture_manager_internal.hpp"

#include "../msg_pipe_factory.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace svanes {

NetworkClient::NetworkClient(ClientSettings settings)
    : settings(std::move(settings)) {}

NetworkClient::~NetworkClient() = default;

void NetworkClient::Connect(const std::string &receive_address,
                            const std::string &server_receive_address) {
    if (receiver || sender) {
        throw std::logic_error("Network client already has a connection.");
    }

    auto new_receiver = internal::MakePipe();
    new_receiver->Listen(receive_address);
    auto new_sender = internal::MakePipe();
    new_sender->Connect(server_receive_address);
    receiver = std::move(new_receiver);
    sender = std::move(new_sender);
}

void NetworkClient::Send(const NetworkMessage &message) {
    if (!sender) {
        throw std::logic_error("Network client has not connected to a server.");
    }
    outgoing.push_back(message);
}

std::span<const NetworkMessage> NetworkClient::PollMessages() const {
    return incoming;
}

void NetworkClient::ReceiveMessages() {
    if (!receiver) {
        return;
    }

    NetworkMessage message;
    while (receiver->Receive(message)) {
        incoming.push_back(std::move(message));
    }
}

void NetworkClient::SendMessages() {
    for (const NetworkMessage &message : outgoing) {
        if (!sender->Send(message)) {
            throw std::runtime_error(
                "Network client could not send a message.");
        }
    }
    outgoing.clear();
}

void NetworkClient::RunClientLoop(IClient &client, SDL_Window *window,
                                  SDL_Renderer *renderer) {

    // First time setup.

    TextureManager texture_manager =
        internal::TextureManagerInternal::Create(renderer);
    AudioManager audio_manager = internal::AudioManagerInternal::Create();
    FontManager font_manager = internal::FontManagerInternal::Create();
    internal::TextLabelRenderer text_label_renderer{texture_manager,
                                                    font_manager};
    internal::RenderQueueExecutor render_queue_executor{renderer,
                                                        texture_manager};
    RenderQueue render_queue;
    InputManager input;
    Camera2D camera;
    std::int32_t output_width = 0;
    std::int32_t output_height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer, &output_width,
                                        &output_height)) {
        throw std::runtime_error("Could not get render output dimensions: " +
                                 std::string{SDL_GetError()});
    }
    camera.SetOutputSize(output_width, output_height);
    ClientContext context{world,        *this,  texture_manager, audio_manager,
                          font_manager, camera, output_width,    output_height};

    // Custom initialization of the client. Implemented by the user of the
    // engine.
    client.Initialize(context);

    // Send whatever first time messages may have been queued during
    // initialization
    SendMessages();

    // fixed origin for measuring elapsed time, so truncation to whole
    // microseconds does not discard part of a microsecond on every frame.
    const auto start_time = std::chrono::steady_clock::now();

    // The previous time in tics, used to calculate the delta time for each
    // render frame.
    TicCount previous_tics = 0;

    bool running = !client.ShouldQuit();
    while (running) {
        const auto frame_start = std::chrono::steady_clock::now();

        //
        // INPUT DETECTION AND PROCESSING
        //

        internal::InputManagerInternal::BeginFrame(input);

        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            internal::InputManagerInternal::HandleEvent(input, event);
        }

        if (!running) {
            break;
        }

        // Gather this frame's server messages before the client reads them.
        // Messages arriving during Update will have to wait for the next frame.
        ReceiveMessages();

        //
        // DELTA TIME CALCULATION
        //
        const TicCount current_tics = static_cast<TicCount>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start_time)
                .count());

        const TicCount real_delta_tics = current_tics - previous_tics;
        previous_tics = current_tics;


        //
        // CLIENT STATE UPDATE
        //

        if (!SDL_GetCurrentRenderOutputSize(renderer, &output_width,
                                            &output_height)) {
            throw std::runtime_error(
                "Could not get render output dimensions: " +
                std::string{SDL_GetError()});
        }
        camera.SetOutputSize(output_width, output_height);

        const ClientFrameContext frame_context{
            world,        *this,         input,         real_delta_tics,
            output_width, output_height, audio_manager, camera};

        // This will call the client's Update method, which should read input,
        // process server messages, and queue any messages to send to the
        // server.
        client.Update(frame_context);

        // Send any messages queued during the client's Update method to the
        // server.
        SendMessages();

        // Once messages have been processed, we don't want to keep
        // applying them unless they're explicitly sent again.
        incoming.clear();

        internal::InputManagerInternal::SynchronizeTextInput(input, window);

        if (client.ShouldQuit()) {
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
        SubmitTileMaps(world, render_queue, camera);
        SubmitRadialGradients(world, render_queue, camera);

        render_queue_executor.Execute(
            render_queue,
            // Our clip rectangle is only relevant when we are in proportional
            // scaling mode.
            camera.scale_mode == ScaleMode::Proportional
                ? std::optional{camera.Viewport()}
                : std::nullopt);

        //
        // USER INTERFACE RENDERING
        //

        // We clear the render queue then submit user interface elements to
        // render them on top of the game world.
        render_queue.Reset();
        text_label_renderer.Submit(world, render_queue, camera.Viewport());
        render_queue_executor.Execute(render_queue, camera.Viewport());

        SDL_RenderPresent(renderer);

        // Implicit limit to 1000 FPS to avoid essentially just busy waiting
        // wasting CPU resources.
        SDL_Delay(1);

        // How long to sleep until the next frame
        const auto interval = client.GetFrameIntervalTics();
        if (interval >
            static_cast<TicCount>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::duration::max())
                    .count())) {
            throw std::overflow_error("Frame interval is too large.");
        }

        // If we are supposed to limit the pace of the main loop, we sleep for
        // the remaining time until the next frame.
        if (running && interval > 0) {
            const auto elapsed = std::chrono::steady_clock::now() - frame_start;
            const auto remaining =
                std::chrono::microseconds(interval) - elapsed;
            if (remaining > std::chrono::steady_clock::duration::zero()) {
                std::this_thread::sleep_for(remaining);
            }
        }
    }
}

std::int32_t NetworkClient::run(IClient &client) {

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

    // This is where the actual logic of the client loop is executed.
    try {
        RunClientLoop(client, window, renderer);
    } catch (...) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw;
    }

    // Cleanup

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace svanes
