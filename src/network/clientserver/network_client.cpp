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
#include "protocol.hpp"

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

void NetworkClient::Connect(const std::string &server_address,
                            TicCount timeout_tics) {
    if (pipe) {
        throw std::logic_error(
            "Network client is already joining or connected.");
    }

    if (timeout_tics == 0) {
        throw std::invalid_argument(
            "A join attempt requires a positive timeout.");
    }

    // The client sends first, so the system can choose any local port.
    auto new_pipe = internal::MakePipe();
    new_pipe->Listen("*:0");
    pipe = std::move(new_pipe);
    this->server_address = server_address;
    join_timeout_tics = timeout_tics;
}

bool NetworkClient::IsConnected() const { return client_id != 0; }

void NetworkClient::Send(const NetworkMessage &message) {
    if (!pipe) {
        throw std::logic_error("Network client has not connected to a server.");
    }
    outgoing.push_back(message);
}

std::span<const NetworkMessage> NetworkClient::PollMessages() const {
    return incoming;
}

void NetworkClient::CompleteJoin(IClient &client, const JoinResult &result) {
    join_started.reset();

    if (result.status == JoinStatus::Accepted) {
        client_id = result.client_id;
    } else {
        pipe.reset();
        server_address.clear();
        outgoing.clear();
    }

    client.HandleJoinResult(result);
}

void NetworkClient::ReceiveMessages(IClient &client) {
    // If we haven't configured a pipe, there is nothing to poll.
    if (!pipe) {
        return;
    }

    // Case 1: Not connected.
    if (!IsConnected()) {
        // Case 1a: Not connected and not yet started joining.
        // So we send the join request and start the timeout timer.
        if (!join_started) {
            const auto request =
                internal::EncodeJoinRequest(client.CreateJoinRequest());

            if (!pipe->Send(server_address, request)) {
                throw std::runtime_error(
                    "Network client could not send its join request.");
            }

            join_started = std::chrono::steady_clock::now();
        } else {
            // Case 1b: Not connected and already started joining.
            // So we check for timeout. If it has expired, we notify the game
            // and return. Otherwise we continue polling for the join response.
            const auto elapsed = static_cast<TicCount>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - *join_started)
                    .count());

            if (elapsed >= join_timeout_tics) {
                CompleteJoin(client, {JoinStatus::TimedOut, 0, {}});
                return;
            }
        }
    }

    // The source address of the current message. Currently ignored.
    std::string address;

    // Case 2: Either we're connected or we're still waiting for a join
    // response.
    NetworkMessage message;
    while (pipe->Receive(address, message)) {
        // An empty message has no engine message kind to read, so skip it.
        if (message.bytes.empty()) {
            continue;
        }

        // The first byte tells us what kind of engine message this is.
        // The remaining fields depend on that kind.
        MessageReader reader(message);
        const auto kind =
            static_cast<internal::ClientServerMessageKind>(reader.ReadUint8());

        // Case 2a: Still joining, and the server has accepted us.
        // So we read our assigned ID and pass the game's response bytes to
        // HandleJoinResult. Zero is reserved for a client that isn't connected.
        if (!IsConnected() &&
            kind == internal::ClientServerMessageKind::Accepted) {
            const auto assigned_id = reader.ReadUint32();
            if (assigned_id == 0) {
                throw std::invalid_argument(
                    "The server assigned client ID zero.");
            }

            CompleteJoin(client, {JoinStatus::Accepted, assigned_id,
                                  internal::ReadPayload(reader)});
            // Leave any remaining messages for the next frame now that the
            // game has been notified and the connection state has changed.
            return;
        }

        // Case 2b: Still joining, and the server has rejected us.
        // So we release the failed connection and notify the game with the
        // rejection response. The callback may start another join attempt,
        // which we will handle on the next frame.
        if (!IsConnected() &&
            kind == internal::ClientServerMessageKind::Rejected) {
            CompleteJoin(client, {JoinStatus::Rejected, 0,
                                  internal::ReadPayload(reader)});
            return;
        }

        // Case 2c: Connected, and this is a gameplay message.
        // So we remove the engine fields and store only the game's bytes for
        // Update to read through PollMessages.
        if (IsConnected() && kind == internal::ClientServerMessageKind::Game) {
            incoming.push_back(internal::ReadPayload(reader));
        }
    }
}

void NetworkClient::SendMessages() {
    if (!IsConnected()) {
        return;
    }

    for (const NetworkMessage &message : outgoing) {
        if (!pipe->Send(server_address, internal::EncodeGameMessage(message))) {
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
        ReceiveMessages(client);

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
