#pragma once

#include <svanes/network/network_message.hpp>

#include <string>

namespace svanes {

/**
 * Represents one local endpoint of a unidirectional message pipe.
 *
 * Message pipes give an API to implement a digraph of message routes. Visualize
 * each application as a vertex, and then a MsgPipe is a directed edge that can
 * be used to send messages from one vertex to another.
 *
 * A successful Listen configures an object to receive only. A successful
 * Connect configures an object to send only. Calling Send on a receiving object
 * or Receive on a sending object throws std::logic_error. Before successful
 * setup and after Close, both Send and Receive throw std::logic_error.
 *
 * For bidirectional communication you might have a setup like so:
 * Node A calls Connect to B at a known IP address and port. Node B calls Listen
 * on *:PORT to receive messages from anyone (including A) at that port.
 * The reverse edge uses a separate MsgPipe object. In the reverse case, B will
 * call Connect to A at a known IP address and port, and A will call Listen on
 * *:PORT to receive messages from anyone (including B) at that port.
 */
class MsgPipe {
public:
    /**
     * Virtual destructor to allow derived classes to clean up resources.
     */
    virtual ~MsgPipe() = default;

    /**
     * Starts listening for incoming connections on the specified address.
     * The receiving direction is established only if this call succeeds.
     *
     * @param address The address to listen on. The format of the address is
     * implementation-defined.
     */
    virtual void Listen(const std::string &address) = 0;

    /**
     * Connects to a remote address. The format of the address is
     * implementation-defined. The sending direction is established only if this
     * call succeeds.
     *
     * @param address The address to connect to.
     */
    virtual void Connect(const std::string &address) = 0;

    /**
     * Sends a message to the connected peer.
     *
     * @param message The message to send.
     *
     * @return True if the message was accepted for sending, false otherwise.
     *
     * @throws std::logic_error if the pipe is configured to receive, has not
     * successfully connected, or has been closed.
     */
    virtual bool Send(const NetworkMessage &message) = 0;

    /**
     * Receives a message from the connected peer.
     *
     * @param message The message to receive.
     * @return True if a message was received, false otherwise.
     * @throws std::logic_error if the pipe is configured to send, has not
     * successfully started listening, or has been closed.
     */
    virtual bool Receive(NetworkMessage &message) = 0;

    /**
     * Closes the message pipe, releasing any resources associated with it.
     * Idempotent in that calling Close multiple times has no additional effect.
     * After closing, both Send and Receive throw std::logic_error.
     */
    virtual void Close() = 0;
};

} // namespace svanes
