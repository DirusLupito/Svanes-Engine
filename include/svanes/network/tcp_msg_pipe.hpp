#pragma once

#include <svanes/network/msg_pipe.hpp>

#include <zmq.hpp>

#include <string>

namespace svanes {

/**
 * Concrete implementation of a MsgPipe using TCP protocol.
 * Uses a ZMQ_PULL socket to receive and a ZMQ_PUSH socket to send raw
 * messages, one message per frame.
 *
 * Call Listen, Connect, Send, Receive, and Close on the same thread.
 */
class TcpMsgPipe final : public MsgPipe {
public:
    /**
     * Creates a TCP message pipe that has not started listening or connected.
     */
    TcpMsgPipe() = default;

    /**
     * Starts listening for incoming TCP connections on the specified address.
     * The receiving direction is established only if this call succeeds.
     * Assumes one sending pipe per listening pipe. Traffic from
     * additional senders is undefined behavior.
     *
     * @param address The address to listen on, in HOST:PORT form. Use *:PORT
     * to listen on all local interfaces.
     *
     * @throws std::logic_error if the pipe is already configured. Call Close
     * before configuring it again.
     *
     * @throws zmq::error_t if the listening socket cannot be configured.
     */
    void Listen(const std::string &address) override;

    /**
     * Connects to a remote listening pipe at the specified address.
     * The sending direction is established only if this call succeeds.
     *
     * @param address The address to connect to, in HOST:PORT form. HOST may
     * be an IPv4 address or a hostname.
     *
     * @throws std::logic_error if the pipe is already configured. Call Close
     * before configuring it again.
     *
     * @throws zmq::error_t if the sending socket cannot be configured,
     * including when the address is malformed.
     */
    void Connect(const std::string &address) override;

    /**
     * Sends a message to the connected peer as one frame.
     * Nonblocking, so this could return false if 0mq's internal buffers are
     * full. Messages can be queued before the TCP connection has been made.
     *
     * @param message The message to send.
     *
     * @return True if the message was accepted for sending, false otherwise.
     * Acceptance does not confirm that the message arrived.
     *
     * @throws std::logic_error if the pipe is configured to receive, has not
     * successfully connected, or has been closed.
     *
     * @throws zmq::error_t if sending through the socket fails.
     */
    bool Send(const NetworkMessage &message) override;

    /**
     * Receives one message from the connected sending pipe.
     * Non-blocking, false indicates that no message was available.
     *
     * @param message The message to receive. Unchanged if none is available.
     *
     * @return True if a message was received, false otherwise.
     *
     * @throws std::logic_error if the pipe is configured to send, has not
     * successfully started listening, or has been closed.
     *
     * @throws zmq::error_t if receiving through the socket fails.
     */
    bool Receive(NetworkMessage &message) override;

    /**
     * Closes the message pipe, releasing the socket and dropping its TCP
     * connections. The context is kept for subsequent setup.
     * Idempotent in that calling Close multiple times has no additional effect.
     * After closing, both Send and Receive throw std::logic_error.
     */
    void Close() override;

private:
    // The ZeroMQ context used to create this pipe's socket.
    zmq::context_t context;

    // The local socket, available after successful Listen or Connect.
    zmq::socket_t socket;
};

} // namespace svanes
