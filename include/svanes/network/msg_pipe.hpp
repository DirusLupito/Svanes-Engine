#pragma once

#include <svanes/network/network_message.hpp>

namespace svanes {

/**
 * Abstraction for a network message pipe to send messages between
 * clients and servers. All concrete implementations of this interface
 * must include an implementation of the Send and Receive functions.
 * Any code that uses a message pipe should be able to switch between
 * different types of message pipes fairly seamlessly.
 */
class MsgPipe {
public:
    /**
     * Virtual destructor for deleting a message pipe.
     */
    virtual ~MsgPipe() = default;

    /**
     * Handles sending a message through a pipe.
     * Checks and verification are the responsibility of the concrete implementations.
     * 
     * @param message The content to be sent through the message pipe.
     */
    virtual bool Send(const NetworkMessage &message) = 0;

    /**
     * Handles receiving a message through a pipe.
     * Checks and verification are the responsibility of the concrete implementations.
     * 
     * @param message The content to be received through the message pipe.
     */
    virtual bool Receive(NetworkMessage &message) = 0;
};

} // namespace svanes
