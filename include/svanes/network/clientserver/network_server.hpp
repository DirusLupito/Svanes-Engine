#pragma once

#include <svanes/network/network_message.hpp>
#include <svanes/registry.hpp>

#include <cstdint>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace svanes {

class IServer;

/**
 * The ID assigned to a client by the server.
 */
using ClientId = std::uint32_t;

/**
 * Represents a single input message received from a client.
 *
 * FIELDS:
 * - client: The unique identifier of the client that sent the input.
 * - message: The input message received from the client.
 */
struct ClientInput {
    ClientId client;
    NetworkMessage message;
};

/**
 * Represents the main application that runs the server loop in a client-server
 * networked game and manages the networking and game state updates for a
 * server. It is responsible for managing client sessions, receiving inputs,
 * sending messages, and running the server loop until the server indicates that
 * it should quit.
 *
 * Uses two separate threads:
 * A communication thread which owns the pipe and exchanges message batches with
 * it, and A main thread which runs the game-facing methods, callbacks, and the
 * server loop.
 */
class NetworkServer final {
public:
    /**
     * Constructs a NetworkServer instance and starts its communication thread.
     */
    NetworkServer();

    /**
     * Stops and joins the communication thread before releasing the server's
     * resources.
     */
    ~NetworkServer();

    /**
     * Requests that the communication thread start the shared pipe for all
     * messages to and from clients. Binding happens on that thread.
     *
     * @param address The local address for all client messages.
     * The format of the address is implementation-defined.
     * @throws std::logic_error if listening has already been requested.
     */
    void Listen(const std::string &address);

    /**
     * Queues a message for sending to the specified client.
     *
     * @param client The unique identifier of the client to send the message
     * to.
     * @param message The message to send to the client.
     *
     * @throws std::out_of_range if the client ID is unknown or not admitted.
     */
    void Send(ClientId client, const NetworkMessage &message);

    /**
     * Queues a message for sending to all connected clients.
     *
     * @param message The message to broadcast to all clients.
     */
    void Broadcast(const NetworkMessage &message);

    /**
     * Polls for input messages received from all clients.
     * Should be used by IServer implementations to process client
     * inputs during the server's update loop.
     *
     * @return A span of ClientInput structures representing the input messages
     * received from clients.
     */
    std::span<const ClientInput> PollInputs() const;

    /**
     * Polls for input messages received from a specific client.
     *
     * @param client The unique identifier of the client to poll for inputs.
     * @return A span of ClientInput structures representing the input messages
     * received from the specified client.
     */
    auto PollInputs(ClientId client) const {

        // get the collection of all inputs for this tick, pass it
        // to a filter view that only returns inputs for the specified client
        // will not copy the inputs, instead returning a view of the original
        // collection
        return PollInputs() |
               std::views::filter([client](const ClientInput &input) {
                   return input.client == client;
               });
    }

    /**
     * Runs the application with the provided server instance. This method
     * initializes the server, manages client sessions, and enters the server
     * loop until the server indicates that it should quit. It returns an
     * integer status code, where 0 indicates success and any non-zero value
     * indicates an error.
     *
     * @param server The server instance to run within the application.
     * @return An integer status code indicating the result of the application
     * run.
     */
    std::int32_t run(IServer &server);

private:
    /**
     * Represents a message and the remote address it is sent to or received
     * from.
     *
     * FIELDS:
     * - address: The destination of an outgoing message or source of an
     * incoming message.
     * - message: The complete message, including the engine protocol fields.
     */
    struct AddressedMessage {
        std::string address;
        NetworkMessage message;
    };

    /**
     * Represents a single client session managed by the server.
     *
     * FIELDS:
     * - address: The client's address, as reported by the pipe when its join
     * request arrived.
     * - outgoing: A vector of NetworkMessage instances representing messages
     * queued for sending to the client.
     * - accepted: Whether the game admitted this client.
     */
    struct Session {
        std::string address;
        std::vector<NetworkMessage> outgoing;
        bool accepted;
    };

    /**
     * Collects messages from the communication thread, handles join callbacks,
     * and collects gameplay messages from accepted clients for the next
     * simulation tick.
     *
     * @param server The game server handling join requests.
     * @param pending A reference to a vector where received ClientInput
     * structures will be stored.
     */
    void ReceiveMessages(IServer &server, std::vector<ClientInput> &pending);

    /**
     * Transfers all queued messages to the communication thread for sending.
     * Called each loop iteration after any simulation work, so admission
     * responses are queued even when a simulation step is not performed.
     */
    void SendMessages();

    /**
     * Runs the server's communication loop on the communication thread, sending
     * queued outgoing messages and collecting incoming messages for the main
     * thread until stopping is requested.
     *
     * @param stop The stop token used to request termination of the
     * communication loop.
     */
    void Communicate(std::stop_token stop);

    // The registry containing all entities and their components for the server.
    Registry world;

    // Collection of client sessions, including rejected join requests.
    // The index of the session corresponds to the client ID minus one
    // (i.e., client ID = index + 1). Rejected requests occupy an entry with
    // accepted = false, so admitted client IDs can have gaps.
    std::vector<Session> sessions;

    // The client ID of each session, keyed by its address, including rejected
    // requests.
    std::unordered_map<std::string, ClientId> clients;

    // A vector of ClientInput structures representing input messages received
    // from admitted clients during the current update tick.
    std::vector<ClientInput> inputs;

    // The mutex protecting the listen request and outgoing messages.
    std::mutex outgoing_mutex;

    // The local address requested by Listen, absent until Listen is called.
    std::optional<std::string> listen_address;

    // Messages waiting for the communication thread to collect and send them.
    std::vector<AddressedMessage> outgoing;

    // The mutex protecting incoming messages.
    std::mutex incoming_mutex;

    // Messages received by the communication thread, awaiting main-thread
    // admission handling or collection as gameplay inputs.
    std::vector<AddressedMessage> incoming;

    // The thread that runs the server's communication loop.
    // This should be the last declared member of the class as its construction
    // will immediately start the thread, which may access other members of the
    // class that we need to ensure are initialized before the thread starts
    // running.
    std::jthread communication_thread;
};

} // namespace svanes
