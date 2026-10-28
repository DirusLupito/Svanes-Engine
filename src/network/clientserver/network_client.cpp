#include <svanes/network/clientserver/network_client.hpp>

#include <svanes/audio/audio_manager.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/network/clientserver/IClient.hpp>
#include <svanes/render/render_queue.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/tilemaps/tilemap.hpp>

#include "../../MenuUtilities/font_manager_internal.hpp"
#include "../../MenuUtilities/text_label_renderer.hpp"
#include "../../audio/audio_manager_internal.hpp"
#include "../../input/input_manager_internal.hpp"
#include "../../render/render_queue_executor.hpp"
#include "../../render/texture_manager_internal.hpp"

#include "../msg_pipe_factory.hpp"
#include "protocol.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace svanes {

NetworkClient::NetworkClient(ClientSettings settings)
    : settings(std::move(settings)),
      communication_thread(
          [this](std::stop_token stop) { Communicate(stop); }) {}

NetworkClient::~NetworkClient() {
    communication_thread.request_stop();
    communication_thread.join();
}

void NetworkClient::Connect(const std::string &target_server_address,
                            TicCount timeout_tics) {
    if (connection_requested) {
        throw std::logic_error(
            "Network client has already requested a connection.");
    }

    if (timeout_tics == 0) {
        throw std::invalid_argument(
            "A join request requires a positive timeout.");
    }

    join_timeout_tics = timeout_tics;
    connection_requested = true;

    // The communication thread will set up the pipe for this join request.
    std::lock_guard lock(outgoing_mutex);
    server_address = target_server_address;
}

void NetworkClient::Disconnect() {
    if (!server_address) {
        return;
    }

    // No more outgoing messages
    outgoing.clear();

    // and no more join data.
    join_started.reset();


    {
        std::lock_guard lock(outgoing_mutex);

        // we will send one single message, a disconnect notice,
        // and then stop the communication thread without any
        // other messages queued

        outgoing_messages.clear();
        if (IsConnected()) {
            outgoing_messages.push_back(internal::EncodeDisconnect(client_id));
        }
        server_address.reset();
    }

    // Clean up our client state and stop communication.

    client_id = 0;
    communication_thread.request_stop();
}

bool NetworkClient::IsConnected() const { return client_id != 0; }

void NetworkClient::Send(const NetworkMessage &message) {
    if (!server_address) {
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
        last_received = std::chrono::steady_clock::now();
        last_heartbeat = last_received;
    } else {
        outgoing.clear();

        // Stop communication and discard queued messages now that the join
        // has been rejected or timed out. The pipe is released when the
        // communication thread exits.
        communication_thread.request_stop();
        std::lock_guard lock(outgoing_mutex);
        server_address.reset();
        outgoing_messages.clear();
    }

    client.HandleJoinResult(result);
}

void NetworkClient::ReceiveMessages(IClient &client) {
    // If no connection has been requested, there is nothing to poll.
    if (!server_address) {
        return;
    }

    // Case 1: Not connected.
    if (!IsConnected()) {
        // Case 1a: No join request has been queued.
        // So we queue the join request and start the timeout timer.
        if (!join_started) {
            auto request =
                internal::EncodeJoinRequest(client.CreateJoinRequest());

            std::lock_guard lock(outgoing_mutex);
            outgoing_messages.push_back(std::move(request));
            join_started = std::chrono::steady_clock::now();
        } else {
            // Case 1b: A join request is pending.
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

    // The batch of messages received since the main thread last collected
    // messages.
    std::vector<NetworkMessage> received;

    {
        std::lock_guard lock(incoming_mutex);
        received.swap(incoming_messages);
    }

    // For each message in the batch, we handle its join result, refresh the
    // connection's timeout, or collect its gameplay payload for the current
    // frame. The communication thread can receive more messages while we
    // process this batch, they'll be collected in the next iteration.
    for (const auto &message : received) {
        // An empty message has no engine message kind to read, so skip it.
        if (message.bytes.empty()) {
            continue;
        }

        // The decoded message from the raw NetworkMessage.
        auto decoded = internal::DecodeMessage(message);

        // Case 2a: Still joining, and the server has accepted us.
        // So we use our assigned ID and pass the game's response bytes to
        // HandleJoinResult. Zero is reserved for a client that isn't connected.
        if (!IsConnected() &&
            decoded.kind == internal::ClientServerMessageKind::Accepted) {
            if (decoded.client_id == 0) {
                throw std::invalid_argument(
                    "The server assigned client ID zero.");
            }

            CompleteJoin(client, {JoinStatus::Accepted, decoded.client_id,
                                  std::move(decoded.payload)});
            // Now that the game has been notified, the remaining messages in
            // this batch can be collected for this frame's Update.
            continue;
        }

        // Case 2b: Still joining, and the server has rejected us.
        // So we request cleanup of the failed connection and notify the game
        // with the rejection response.
        if (!IsConnected() &&
            decoded.kind == internal::ClientServerMessageKind::Rejected) {
            CompleteJoin(client,
                         {JoinStatus::Rejected, 0, std::move(decoded.payload)});
            return;
        }

        // Case 2c: Connected, and the server sent gameplay or a heartbeat.
        // Either confirms that the connection is still alive, but only the
        // gameplay payload is passed to Update through PollMessages.
        if (IsConnected() &&
            (decoded.kind == internal::ClientServerMessageKind::Game ||
             decoded.kind == internal::ClientServerMessageKind::Heartbeat)) {
            last_received = std::chrono::steady_clock::now();
            if (decoded.kind == internal::ClientServerMessageKind::Game) {
                incoming.push_back(std::move(decoded.payload));
            }
        }
    }

    // Check for a connection timeout after processing the batch of messages. If
    // the server has not sent a heartbeat or gameplay message within the
    // timeout interval, we disconnect and notify the client implementation.
    if (IsConnected() && std::chrono::steady_clock::now() - last_received >=
                             internal::ConnectionTimeout) {
        std::cerr << "Network client: server connection timed out.\n";
        Disconnect();
        client.HandleDisconnect();
    }
}

void NetworkClient::SendMessages() {
    if (!IsConnected()) {
        return;
    }

    // The gameplay messages and any heartbeat queued by the current frame.
    std::vector<NetworkMessage> batch;

    for (const NetworkMessage &message : outgoing) {
        batch.push_back(internal::EncodeGameMessage(message, client_id));
    }
    outgoing.clear();

    // If it has been more than HeartbeatInterval since the last heartbeat,
    // queue a heartbeat message to keep the connection alive.
    const auto now = std::chrono::steady_clock::now();
    if (now - last_heartbeat >= internal::HeartbeatInterval) {
        batch.push_back(internal::EncodeHeartbeat(client_id));
        last_heartbeat = now;
    }

    // Append rather than replace, as the communication thread may not have
    // collected the previous frame's messages yet.
    std::lock_guard lock(outgoing_mutex);
    outgoing_messages.insert(outgoing_messages.end(),
                             std::make_move_iterator(batch.begin()),
                             std::make_move_iterator(batch.end()));
}

void NetworkClient::Communicate(std::stop_token stop) {
    // The pipe used to communicate with the server, created and destroyed on
    // this thread.
    std::unique_ptr<MsgPipe> pipe;

    // The server's address, absent until Connect requests a connection.
    std::optional<std::string> address;

    // The outgoing batch of messages owned by this thread while sending.
    std::vector<NetworkMessage> sending;

    // The incoming batch of messages owned by this thread while receiving.
    std::vector<NetworkMessage> received;

    while (true) {
        // Collect the server's address if the pipe hasn't been created, and
        // transfer queued outgoing messages to the sending batch. The main
        // thread can queue the next batch once this lock is released.
        {
            std::lock_guard lock(outgoing_mutex);
            // A disconnect notice may still be queued when the frame loop
            // ends. Finish sending it before releasing the pipe.
            if (stop.stop_requested() && outgoing_messages.empty()) {
                break;
            }

            if (!pipe) {
                address = server_address;
            }
            sending.swap(outgoing_messages);
        }

        // If a server address has been supplied, set up the pipe once.
        if (!pipe && address) {
            pipe = internal::MakePipe();

            // The client sends first, so the system can choose any local port.
            pipe->Listen("*:0");
        }

        // If the pipe is available, send the queued messages.
        if (pipe) {
            for (const auto &message : sending) {
                if (!pipe->Send(*address, message)) {
                    throw std::runtime_error(
                        "Network client could not send a message.");
                }
            }

            // Clear the sending batch after sending all messages, so we don't
            // resend them in the next iteration.
            sending.clear();

            // The source address of the current message. Currently ignored.
            std::string source_address;

            // The current datagram, reused for each receive.
            NetworkMessage message;

            // Receive messages from the pipe until there are no more, or until
            // a stop is requested.
            while (!stop.stop_requested() &&
                   pipe->Receive(source_address, message)) {
                received.push_back(std::move(message));
            }

            // Transfer the received messages to the main thread's incoming
            // batch, preserving any messages it has not collected yet.
            {
                std::lock_guard lock(incoming_mutex);
                incoming_messages.insert(
                    incoming_messages.end(),
                    std::make_move_iterator(received.begin()),
                    std::make_move_iterator(received.end()));
            }

            // Clear the received batch after transferring, so we don't double
            // add them in the next iteration.
            received.clear();
        }

        // 1 millisecond sleep to avoid infinitely tight loop wasting CPU cycles
        // when there is no work to do.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
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
    EventManager events;
    internal::InputManagerInternal::RegisterEventHandlers(input, events);
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
        internal::InputManagerInternal::Poll(input, events, window);
        events.DispatchEvents();

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
