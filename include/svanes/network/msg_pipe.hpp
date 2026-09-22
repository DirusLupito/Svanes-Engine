#pragma once

#include <svanes/network/network_message.hpp>

#include <string>
#include <vector>

namespace svanes {

/**
 * A unique identifier for a connection in a message pipe.
 *
 * FIELDS:
 * - value: The local connection id. Zero is invalid.
 */
struct ConnectionId {
    std::uint32_t value = 0;

    /**
     * @return Whether the two connection ids have the same value.
     */
    bool operator==(const ConnectionId &) const = default;
};

/**
 * The received message including the source connection id and the message itself.
 *
 * FIELDS:
 * - source: The connection that sent the message.
 * - message: The received bytes.
 */
struct ReceivedMessage {
    ConnectionId source;
    NetworkMessage message;
};

/**
 *
 */
class MsgPipe {
public:
    /**
     *
     */
    virtual ~MsgPipe() = default;

    /**
     * 
     */
    bool Send(const NetworkMessage &message);

    /**
     * 
     */
    bool Receive(NetworkMessage &message);

    /**
     * Sends a message to the given connection id.
     *
     * @param destination The connection to send to.
     * @param message The bytes to send.
     * @return Whether the transport accepted the send, not confirmation of delivery.
     * @throws std::invalid_argument if the connection id is unknown.
     */
    virtual bool Send(ConnectionId destination,
                      const NetworkMessage &message) = 0;

    /**
     * Receives a message from any connection, returning the source connection id
     * and the message itself.
     *
     * @param received Receives the source and bytes when a message is available.
     * @return Whether a message was received. False means none is available.
     */
    virtual bool Receive(ReceivedMessage &received) = 0;

    /**
     * Returns a list of all known connection ids.
     *
     * @return The connection ids belonging to this pipe.
     */
    std::vector<ConnectionId> Connections() const;

protected:
    /**
     * Stores a connection route and returns its connection id. If the route is
     * already known, the existing connection id is returned.
     *
     * @param route The transport address or routing identity to store.
     * @return The connection id assigned to the route.
     * @throws std::overflow_error if no more connection ids are available.
     */
    ConnectionId RememberConnection(const std::string &route);

    /**
     * Gets the route for a given connection id.
     *
     * @param connection The connection to look up.
     * @return The stored transport address or routing identity.
     * @throws std::invalid_argument if the connection id is unknown.
     */
    const std::string &Route(ConnectionId connection) const;

private:
    /**
     * Stores the routes for all known connections.
     */
    std::vector<std::string> routes;
};

} // namespace svanes
