#pragma once

#include <svanes/network/msg_pipe.hpp>

#include <zmq.hpp>

#include <cstdint>
#include <string>

namespace svanes {

/**
 * Concrete implementation of a MsgPipe using TCP protocol.
 * Uses a ZMQ_PULL socket to receive and a ZMQ_PUSH socket to send raw
 * messages, one message per frame.
 *
 * Call Listen, Connect, Port, Send, Receive, and Close on the same thread.
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
     * Any number of connecting pipes may send to one listening pipe.
     *
     * @param address The address to listen on, in HOST:PORT form. Use *:PORT
     * to listen on all local interfaces, and *:0 to let the OS pick an
     * available port (Port returns the chosen port).
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
     * The TCP connection itself is made in the background, so success does
     * not mean that the remote pipe is listening yet.
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
     * Finds the pipe's endpoint port so a joining client can be directed to it.
     *
     * @return The local listening port, or the destination port for a
     * connecting pipe.
     *
     * @throws std::logic_error if the pipe has not successfully started
     * listening or connected, or has been closed.
     *
     * @throws zmq::error_t if the endpoint cannot be queried.
     */
    std::uint16_t Port() const;

    /**
     * Sends a message to the connected peer as one frame.
     * Nonblocking, so this could return false if 0mq's internal buffers are
     * full or the TCP connection has not been made yet.
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
     * Receives one message from any pipe connected to this listening pipe.
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

    // True after a successful Listen, false for a connecting pipe or a pipe
    // that has not been configured, including after Close.
    bool listening = false;
};

} // namespace svanes
