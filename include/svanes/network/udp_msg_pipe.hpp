#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <zmq.hpp>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace svanes {

/**
 * Concrete implementation of a MsgPipe using UDP protocol.
 * Uses a ZMQ_DGRAM socket to send and receive message fragments. Messages
 * are returned only after all of their fragments have arrived.
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
     * Sends a message to the specified address, splitting it into datagrams
     * small enough for ZeroMQ's UDP buffer.
     * Nonblocking, so this could return false if 0mq's internal buffers are
     * full.
     *
     * @param address The address to send to, in numeric IPv4:PORT form.
     * @param message The message to send.
     *
     * @return True if every fragment was accepted for sending, false otherwise.
     * Some fragments may have been sent even when this returns false.
     * Acceptance does not confirm that the message arrived.
     *
     * @throws std::logic_error if the pipe has not successfully started
     * listening, or has been closed.
     *
     * @throws std::invalid_argument if the IPv4 address is invalid or the port
     * is outside the range from 1 to 65535.
     *
     * @throws std::length_error if the message size cannot fit in the
     * fragment header's unsigned 32-bit size field.
     *
     * @throws std::runtime_error if the destination frame was accepted but
     * the datagram body could not be sent.
     *
     * @throws zmq::error_t if sending through the socket fails.
     */
    bool Send(const std::string &address,
              const NetworkMessage &message) override;

    /**
     * Receives a complete message from any sender at the listening endpoint.
     * Nonblocking, false indicates that no complete message was available.
     * Incomplete messages remain available for later fragments and do not
     * prevent other messages from completing. Assemblies older than one
     * second from their first fragment are discarded when this is polled.
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
     * @throws std::invalid_argument if a fragment header or payload is invalid.
     *
     * @throws zmq::error_t if receiving through the socket fails.
     */
    bool Receive(std::string &address, NetworkMessage &message) override;

    /**
     * Closes the message pipe, releasing the socket and incomplete messages.
     * The context is kept for subsequent setup.
     * Idempotent in that calling Close multiple times has no additional effect.
     * After closing, both Send and Receive throw std::logic_error.
     */
    void Close() override;

private:
    /**
     * The fragments received so far for one message from one sender.
     *
     * FIELDS:
     * - size: The complete message's size in bytes.
     * - fragments: The received payloads, keyed by their fragment indices.
     * - started: The time the first fragment arrived, used for expiration.
     */
    struct IncompleteMessage {
        std::uint32_t size;
        std::map<std::uint32_t, std::vector<std::byte>> fragments;
        std::chrono::steady_clock::time_point started;
    };

    // The ZeroMQ context used to create this pipe's socket.
    zmq::context_t context;

    // The local socket, available after successful Listen.
    zmq::socket_t socket;

    // The ID assigned to the next outgoing message, including failed sends.
    std::uint64_t next_message_id = 0;

    // Incomplete messages, identified by the sender's address and message ID.
    std::map<std::pair<std::string, std::uint64_t>, IncompleteMessage>
        incomplete;
};

} // namespace svanes
