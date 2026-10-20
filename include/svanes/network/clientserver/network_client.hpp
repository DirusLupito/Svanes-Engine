#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
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
 */
class NetworkClient final {
public:
    /**
     * Constructs a NetworkClient with the specified window settings.
     *
     * @param settings The title and initial dimensions of the client window.
     */
    explicit NetworkClient(ClientSettings settings = {});

    /**
     * Destroys the client and releases its message pipe.
     */
    ~NetworkClient();

    /**
     * Starts a nonblocking join attempt. 
     * 
     * Frames continue while waiting. Gameplay messages remain queued until
     * acceptance. All pipe operations must remain on the same thread as Connect.
     *
     * @param server_address The remote address where the server listens
     * for admission and gameplay messages.
     * @param timeout_tics The time allowed for a response, in microseconds,
     * starting when the request is sent. Defaults to sixty seconds.
     * 
     * @throws std::logic_error if already connecting or connected.
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
     * Messages queued while joining are discarded if the attempt fails.
     *
     * @param message The message to send to the server.
     * @throws std::logic_error if no join attempt or connection exists.
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
     * Handles the join attempt and receives gameplay messages into incoming.
     * Called before Update so join callbacks and this frame's messages are
     * available to the game without blocking the frame loop.
     *
     * @param client The game client supplying the handshake callbacks.
     */
    void ReceiveMessages(IClient &client);

    /**
     * Updates connection state before delivering the one last callback.
     * Failed attempts release their pipes and discard queued gameplay messages.
     *
     * @param client The game client handling the result.
     * @param result The acceptance, rejection, or timeout outcome.
     */
    void CompleteJoin(IClient &client, const JoinResult &result);

    /**
     * Sends all queued messages to the server. Should be called once per frame
     * after all input has been processed for that frame.
     */
    void SendMessages();

    // The settings for the client's window, including title and dimensions.
    ClientSettings settings;

    // The registry containing all entities and their components for the client.
    Registry world;

    // The message pipe used to send messages to and receive messages from the
    // server.
    std::unique_ptr<MsgPipe> pipe;

    // The ID assigned by the server to this client, or zero if not connected.
    std::uint32_t client_id = 0;

    // The server's address, or empty if not joining or connected.
    std::string server_address;

    // Stores how long the client will wait for a join response, in microseconds.
    TicCount join_timeout_tics = 0;

    // The time point when the join attempt was started, or std::nullopt if not
    // currently connecting. Used to determine if the join attempt has timed out.
    std::optional<std::chrono::steady_clock::time_point> join_started;

    // A vector of NetworkMessage instances representing messages received
    // from the server during the current frame.
    std::vector<NetworkMessage> incoming;

    // A vector of NetworkMessage instances representing messages queued
    // to be sent to the server during the current frame.
    std::vector<NetworkMessage> outgoing;
};

} // namespace svanes
