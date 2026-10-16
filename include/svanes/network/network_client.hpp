#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/registry.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;

namespace svanes {

class IClient;

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
     * Destroys the client and releases its message pipes.
     */
    ~NetworkClient();

    /**
     * Establishes separate receiving and sending pipes for the server session.
     * This can be called before run or from the client's Initialize or Update.
     * The server must be configured with the corresponding addresses.
     *
     * @param receive_address The local address to listen for server messages.
     * @param server_receive_address The remote address where the server listens
     * for this client's messages. Address formats are implementation-defined.
     * @throws std::logic_error if the client already has a connection.
     */
    void Connect(const std::string &receive_address,
                 const std::string &server_receive_address);

    /**
     * Queues a message to send to the server after Initialize or Update.
     *
     * @param message The message to send to the server.
     * @throws std::logic_error if the client has not connected to a server.
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
     * Receives messages from the server and stores them in the incoming vector.
     * This should be called before the client's Update method to ensure that
     * all server messages are available for processing.
     */
    void ReceiveMessages();

    /**
     * Sends all queued messages to the server. Should be called once per frame
     * after all input has been processed for that frame.
     */
    void SendMessages();

    // The settings for the client's window, including title and dimensions.
    ClientSettings settings;

    // The registry containing all entities and their components for the client.
    Registry world;

    // The message pipe used to receive messages from the server.
    std::unique_ptr<MsgPipe> receiver;

    // The message pipe used to send messages to the server.
    std::unique_ptr<MsgPipe> sender;

    // A vector of NetworkMessage instances representing messages received
    // from the server during the current frame.
    std::vector<NetworkMessage> incoming;

    // A vector of NetworkMessage instances representing messages queued
    // to be sent to the server during the current frame.
    std::vector<NetworkMessage> outgoing;
};

} // namespace svanes
