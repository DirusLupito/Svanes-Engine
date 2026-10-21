#pragma once

#include <svanes/network/network_message.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;

namespace svanes {

class IClient;
struct JoinResult;

/**
 * Defines the settings for the client's window.
 *
 * FIELDS:
 * - title: The title displayed by the client window.
 * - width: The initial width of the client window.
 * - height: The initial height of the client window.
 */
struct ClientSettings {
    std::string title = "Svanes Engine";
    std::int32_t width = 1920;
    std::int32_t height = 1080;
};

/**
 * Represents the main application that runs the client loop in a client-server
 * networked game and manages the networking and game state updates for a
 * client. It is responsible for creating the client window, managing the
 * connection to the server, receiving messages, sending messages, and running
 * the client loop until the client indicates that it should quit.
 *
 * Uses two separate threads:
 * A communication thread which sends and receives messages through the pipe,
 * and a main thread which runs the game-facing methods, callbacks, input,
 * rendering, and the client loop.
 */
class NetworkClient final {
public:
    /**
     * Constructs a NetworkClient with the specified window settings and starts
     * its communication thread.
     *
     * @param settings The title and initial dimensions of the client window.
     */
    explicit NetworkClient(ClientSettings settings = {});

    /**
     * Stops and joins the communication thread, then releases the client's
     * resources.
     */
    ~NetworkClient();

    /**
     * Starts a nonblocking join request. May be called once per NetworkClient
     * instance.
     *
     * Frames continue while waiting. Gameplay messages remain queued until
     * acceptance. Pipe setup, sending, and receiving happen on the
     * communication thread.
     *
     * @param server_address The remote address where the server listens
     * for admission and gameplay messages.
     * @param timeout_tics The time allowed for a response, in microseconds,
     * starting when the request is queued for sending. Defaults to sixty
     * seconds.
     *
     * @throws std::logic_error if a connection has already been requested.
     *
     * @throws std::invalid_argument if the timeout is zero.
     */
    void Connect(const std::string &server_address,
                 TicCount timeout_tics = 60 * TicsPerSecond);

    /**
     * Determines whether the client is currently connected to the server.
     * 
     * @return Whether the server has accepted this client.
     */
    bool IsConnected() const;

    /**
     * Queues a message to send to the server after acceptance and Update.
     * Messages queued while joining are discarded on rejection or timeout.
     *
     * @param message The message to send to the server.
     *
     * @throws std::logic_error if there is no pending join or established
     * connection.
     */
    void Send(const NetworkMessage &message);

    /**
     * Returns the server messages collected for the current frame.
     * This should be used by IClient implementations to process server
     * messages during the client's Update method.
     *
     * @return A span of the server's messages for this frame.
     */
    std::span<const NetworkMessage> PollMessages() const;

    /**
     * Creates the window, initializes the client, and runs its frame loop until
     * the window is closed or the client indicates that it should quit.
     *
     * @param client The game client to run.
     * @return Zero on success, or 1 if SDL or window creation fails.
     */
    std::int32_t run(IClient &client);

private:
    /**
     * Runs the client's frame loop, calling the client's Update method and
     * managing the window, input, and networking. Should not be exposed
     * to the user of the engine.
     *
     * @param client The game client to run.
     * @param window The SDL window to render to.
     * @param renderer The SDL renderer to use for rendering.
     */
    void RunClientLoop(IClient &client, SDL_Window *window,
                       SDL_Renderer *renderer);

    /**
     * Handles the pending join and collects gameplay messages from the
     * communication thread into incoming.
     * Called before Update so join callbacks and this frame's messages are
     * available to the game without blocking the frame loop.
     *
     * @param client The game client supplying the handshake callbacks.
     */
    void ReceiveMessages(IClient &client);

    /**
     * Updates connection state before delivering the join result callback.
     * Rejection or timeout stops the communication thread and discards queued
     * gameplay messages.
     *
     * @param client The game client handling the result.
     * @param result The acceptance, rejection, or timeout outcome.
     */
    void CompleteJoin(IClient &client, const JoinResult &result);

    /**
     * Transfers queued gameplay messages to the communication thread after
     * acceptance. Should be called once per frame after all input has been
     * processed for that frame.
     */
    void SendMessages();

    /**
     * Runs the client's communication loop on the communication thread, sending
     * queued outgoing messages and collecting incoming messages for the main
     * thread until stopping is requested.
     *
     * @param stop The stop token used to request termination of the
     * communication loop.
     */
    void Communicate(std::stop_token stop);

    // The settings for the client's window, including title and dimensions.
    ClientSettings settings;

    // The registry containing all entities and their components for the client.
    Registry world;

    // Whether a connection has already been requested for this client.
    bool connection_requested = false;

    // The ID assigned by the server to this client, or zero if not connected.
    std::uint32_t client_id = 0;

    // Stores how long the client will wait for a join response, in microseconds.
    TicCount join_timeout_tics = 0;

    // The time point when the join request was queued, or std::nullopt if no
    // join is pending. Used to determine if the pending join has timed out.
    std::optional<std::chrono::steady_clock::time_point> join_started;

    // A vector of NetworkMessage instances representing messages received
    // from the server during the current frame.
    std::vector<NetworkMessage> incoming;

    // Gameplay messages awaiting acceptance or transfer to the communication
    // thread after Update.
    std::vector<NetworkMessage> outgoing;

    // The mutex protecting connection requests and outgoing messages.
    std::mutex outgoing_mutex;

    // The server's address, or std::nullopt if not joining or connected.
    std::optional<std::string> server_address;

    // Messages waiting to be sent by the communication thread.
    std::vector<NetworkMessage> outgoing_messages;

    // The mutex protecting messages received by the communication thread.
    std::mutex incoming_mutex;

    // Messages received by the communication thread, awaiting main-thread join
    // handling or collection for the next client frame.
    std::vector<NetworkMessage> incoming_messages;

    // The thread that runs the client's communication loop.
    // This should be the last declared member of the class as its construction
    // will immediately start the thread, which may access other members of the
    // class that we need to ensure are initialized before the thread starts
    // running.
    std::jthread communication_thread;
};

} // namespace svanes
