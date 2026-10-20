#pragma once

#include <string>
#include <svanes/network/msg_pipe.hpp>
#include <zmq.hpp>

namespace svanes {

/**
 * Concrete implementation of a MsgPipe using UDP protocol.
 * Uses a ZMQ_DGRAM socket to send and receive raw packets.
 *
 * Call Listen, Send, Receive, and Close on the same thread.
 */
class UdpMsgPipe final : public MsgPipe {
public:
    /**
     * Creates a UDP message pipe that has not started listening.
     */
    UdpMsgPipe() = default;

    /**
     * Binds the specified local address for sending and receiving datagrams.
     * Both directions are established only if this call succeeds.
     * Datagrams may arrive from multiple senders. This pipe does not select
     * or restrict the remote sender.
     *
     * @param address The local address to bind, in IPv4:PORT form. Use
     * *:PORT to listen on all local interfaces, or *:0 to let the system
     * choose the port.
     *
     * @throws std::logic_error if the pipe is already configured. Call Close
     * before configuring it again.
     *
     * @throws zmq::error_t if the listening socket cannot be configured.
     */
    void Listen(const std::string &address) override;

    /**
     * Sends a message to the specified address as one datagram.
     * Nonblocking, so this could return false if 0mq's internal buffers are
     * full.
     *
     * @param address The address to send to, in numeric IPv4:PORT form.
     * @param message The message to send.
     *
     * @return True if the message was accepted for sending, false otherwise.
     * Acceptance does not confirm that the message arrived.
     *
     * @throws std::logic_error if the pipe has not successfully started
     * listening, or has been closed.
     *
     * @throws std::invalid_argument if the IPv4 address is invalid or the port
     * is outside the range from 1 to 65535.
     *
     * @throws std::length_error if the message exceeds the ZeroMQ
     * UDP buffer limit of 8192 bytes.
     *
     * @throws std::runtime_error if the destination frame was accepted but
     * the datagram body could not be sent.
     *
     * @throws zmq::error_t if sending through the socket fails.
     */
    bool Send(const std::string &address,
              const NetworkMessage &message) override;

    /**
     * Receives one datagram from any sender at the listening endpoint.
     * Non-blocking, false indicates that no message was available.
     *
     * @param address Output parameter. Will be set to the sender's address, in
     * numeric IPv4:PORT form. Unchanged if no message is available.
     * @param message Output parameter. Will be set to the message received.
     * Unchanged if none is available.
     *
     * @return True if a message was received, false otherwise.
     *
     * @throws std::logic_error if the pipe has not successfully started
     * listening, or has been closed.
     *
     * @throws std::runtime_error if an address frame arrives without its
     * datagram body.
     *
     * @throws zmq::error_t if receiving through the socket fails.
     */
    bool Receive(std::string &address, NetworkMessage &message) override;

    /**
     * Closes the message pipe, releasing the socket. The context is kept for
     * subsequent setup.
     * Idempotent in that calling Close multiple times has no additional effect.
     * After closing, both Send and Receive throw std::logic_error.
     */
    void Close() override;

private:
    // The ZeroMQ context used to create this pipe's socket.
    zmq::context_t context;

    // The local socket, available after successful Listen.
    zmq::socket_t socket;
};

} // namespace svanes
