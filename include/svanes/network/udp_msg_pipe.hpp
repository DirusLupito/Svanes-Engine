#pragma once

#include <string>
#include <svanes/network/msg_pipe.hpp>
#include <zmq.hpp>

namespace svanes {

/**
 * Concrete implementation of a MsgPipe using UDP protocol.
 * Uses a ZMQ_DGRAM socket to send and receive raw packets.
 *
 * Call Listen, Connect, Send, Receive, and Close on the same thread.
 */
class UdpMsgPipe final : public MsgPipe {
public:
    /**
     * Creates a UDP message pipe that has not started listening or connected.
     */
    UdpMsgPipe() = default;

    /**
     * Starts listening for incoming datagrams on the specified address.
     * The receiving direction is established only if this call succeeds.
     *
     * @param address The address to listen on, in IPv4:PORT form. Use *:PORT
     * to listen on all local interfaces.
     *
     * @throws std::logic_error if the pipe is already configured. Call Close
     * before configuring it again.
     *
     * @throws zmq::error_t if the listening socket cannot be configured.
     */
    void Listen(const std::string &address) override;

    /**
     * Configures the remote address that outgoing datagrams will be sent to.
     * The sending direction is established only if this call succeeds.
     *
     * @param address The address to send to, in numeric IPv4:PORT form.
     *
     * @throws std::logic_error if the pipe is already configured. Call Close
     * before configuring it again.
     *
     * @throws std::invalid_argument if the IPv4 address is invalid or the port
     * is outside the range from 1 to 65535.
     *
     * @throws zmq::error_t if the sending socket cannot be configured.
     */
    void Connect(const std::string &address) override;

    /**
     * Sends a message to the connected peer as one datagram.
     * Nonblocking, so this could return false if 0mq's internal buffers are
     * full.
     *
     * @param message The message to send.
     *
     * @return True if the message was accepted for sending, false otherwise.
     * Acceptance does not confirm that the message arrived.
     *
     * @throws std::logic_error if the pipe is configured to receive, has not
     * successfully connected, or has been closed.
     *
     * @throws std::length_error if the message exceeds the ZeroMQ
     * UDP buffer limit of 8192 bytes.
     *
     * @throws std::runtime_error if the destination frame was accepted but
     * the datagram body could not be sent.
     *
     * @throws zmq::error_t if sending through the socket fails.
     */
    bool Send(const NetworkMessage &message) override;

    /**
     * Receives one datagram from the listening socket.
     * The source address is discarded, leaving only the message bytes.
     * Non-blocking, false indicates that no message was available.
     *
     * @param message The message to receive. Unchanged if none is available.
     *
     * @return True if a message was received, false otherwise.
     *
     * @throws std::logic_error if the pipe is configured to send, has not
     * successfully started listening, or has been closed.
     *
     * @throws std::runtime_error if an address frame arrives without its
     * datagram body.
     *
     * @throws zmq::error_t if receiving through the socket fails.
     */
    bool Receive(NetworkMessage &message) override;

    /**
     * Closes the message pipe, releasing the socket and forgetting its
     * destination. The context is kept for subsequent setup.
     * Idempotent in that calling Close multiple times has no additional effect.
     * After closing, both Send and Receive throw std::logic_error.
     */
    void Close() override;

private:
    // The ZeroMQ context used to create this pipe's socket.
    zmq::context_t context;

    // The local socket, available after successful Listen or Connect.
    zmq::socket_t socket;

    // The normalized IPv4:PORT destination. Empty for a listening pipe or
    // a pipe that has not been configured, including after Close.
    std::string destination;
};

} // namespace svanes
