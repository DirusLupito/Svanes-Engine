#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/registry.hpp>

#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace svanes {

class IServer;

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
 */
class NetworkServer final {
public:
    /**
     * Constructs a NetworkServer instance.
     */
    NetworkServer();

    /**
     * Destroys the NetworkServer instance, releasing any resources associated
     * with it.
     */
    ~NetworkServer();

    /**
     * Adds a new client session to the server with the specified receiving
     * address and the client's receiving address. This establishes a connection
     * for sending and receiving messages with the client.
     *
     * @param receive_address The local address to listen for incoming messages
     * from the client. The format of the address is implementation-defined.
     * @param client_receive_address The remote address of the client to send
     * messages to. The format of the address is implementation-defined.
     * @return The unique identifier of the newly added client session.
     *
     * @throws std::overflow_error if the server has exhausted available client
     * IDs.
     * @throws std::invalid_argument if the receiving address is already
     * registered for another session.
     */
    ClientId AddSession(const std::string &receive_address,
                        const std::string &client_receive_address);

    /**
     * Sends a message to the specified client.
     *
     * @param client The unique identifier of the client to send the message
     * to.
     * @param message The message to send to the client.
     *
     * @throws std::out_of_range if the specified client ID is unknown.
     */
    void Send(ClientId client, const NetworkMessage &message);

    /**
     * Broadcasts a message to all connected clients.
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
     * Represents a single client session managed by the server.
     *
     * FIELDS:
     * - receive_address: The local address to listen for incoming messages
     * from the client.
     * - receiver: A unique pointer to a MsgPipe instance used for receiving
     * messages from the client.
     * - sender: A unique pointer to a MsgPipe instance used for sending
     * messages to the client.
     * - outgoing: A vector of NetworkMessage instances representing messages
     * queued for sending to the client.
     */
    struct Session {
        std::string receive_address;
        std::unique_ptr<MsgPipe> receiver;
        std::unique_ptr<MsgPipe> sender;
        std::vector<NetworkMessage> outgoing;
    };

    /**
     * Receives input messages from all connected clients and stores them in
     * the provided vector of pending inputs.
     *
     * @param pending A reference to a vector where received ClientInput
     * structures will be stored.
     */
    void ReceiveInputs(std::vector<ClientInput> &pending);

    /**
     * Sends all queued messages to their respective clients.
     * This method should be called after the server's update loop to ensure
     * that all outgoing messages are sent to clients.
     *
     * @throws std::runtime_error if a message could not be sent to a client.
     */
    void SendMessages();

    // The registry containing all entities and their components for the server.
    Registry world;

    // A vector of Session structures representing all connected client
    // sessions.
    std::vector<Session> sessions;

    // A vector of ClientInput structures representing input messages received
    // from clients during the current update tick. The client at index i in the
    // sessions vector has client ID i + 1.
    std::vector<ClientInput> inputs;
};

} // namespace svanes
