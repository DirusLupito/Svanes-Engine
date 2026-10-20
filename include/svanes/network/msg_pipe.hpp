#pragma once

#include <svanes/network/network_message.hpp>

#include <string>

namespace svanes {

/**
 * Represents one local endpoint of a bidirectional message pipe.
 *
 * Message pipes give an API to implement a digraph of message routes. Visualize
 * each application as a vertex, and then a MsgPipe as that vertex's mailbox.
 * Each Send names a directed edge to another vertex by its address.
 *
 * A successful Listen binds a local address, after which the object both sends
 * and receives. Before successful setup and after Close, both Send and Receive
 * throw std::logic_error.
 *
 * The pipe does not remember destinations. Callers pass an address to every
 * Send and must only send to addresses that will accept the message, such as
 * one that is port forwarded or one that has already sent to this pipe. Receive
 * reports the address a message came from, so a caller can reply to it. The
 * pipe attaches no meaning to addresses, it just delivers the bytes.
 *
 * For example, a server calls Listen on *:PORT at a known port. A client calls
 * Listen on *:0 to let the system choose its port, then sends to the server's
 * address. The server replies to the address reported by Receive.
 */
class MsgPipe {
public:
    /**
     * Virtual destructor to allow derived classes to clean up resources.
     */
    virtual ~MsgPipe() = default;

    /**
     * Binds the specified local address for sending and receiving messages.
     * Both directions are established only if this call succeeds.
     *
     * @param address The local address to bind. The format of the address is
     * implementation-defined.
     */
    virtual void Listen(const std::string &address) = 0;

    /**
     * Sends a message to the specified address.
     *
     * @param address The address to send to. The format of the address is
     * implementation-defined.
     * @param message The message to send.
     *
     * @return True if the message was accepted for sending, false otherwise.
     *
     * @throws std::logic_error if the pipe has not successfully started
     * listening, or has been closed.
     */
    virtual bool Send(const std::string &address,
                      const NetworkMessage &message) = 0;

    /**
     * Receives a message and the address it came from.
     *
     * @param address Set to the address the message came from. The format of
     * the address is implementation-defined.
     * @param message Set to the message received.
     * @return True if a message was received, false otherwise.
     * @throws std::logic_error if the pipe has not successfully started
     * listening, or has been closed.
     */
    virtual bool Receive(std::string &address, NetworkMessage &message) = 0;

    /**
     * Closes the message pipe, releasing any resources associated with it.
     * Idempotent in that calling Close multiple times has no additional effect.
     * After closing, both Send and Receive throw std::logic_error.
     */
    virtual void Close() = 0;
};

} // namespace svanes
