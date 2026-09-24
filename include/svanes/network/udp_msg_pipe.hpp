#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace svanes {

/**
 * Concrete implementation of a MsgPipe using UDP protocol.
 * Uses ZMQ_DGRAM socket to send raw packets
 */
class UdpMsgPipe final : public MsgPipe {
public:
    /**
     * Constructor for a UDP message pipe with no known peers.
     * Remote host information can be learned upon accepting a message with Receive().
     * 
     * @param local_port The local port that the socket will be bound to.
     */
    explicit UdpMsgPipe(std::uint16_t local_port);

    /**
     * Constructor for a UDP message pipe with a destination peer.
     * 
     * @param local_port The local port that the socket will be bound to.
     * @param remote_host The hostname of the remote peer the socket will be bound to.
     * @param remote_port The port number of the remote peer the socket will be bound to
     */
    UdpMsgPipe(std::uint16_t local_port, const std::string &remote_host,
               std::uint16_t remote_port);

    /**
     * Sends a message and address through a socket to all peers.
     * 
     * @param message The message being sent
     * @return false if there is no peer to send to, otherwise true
     */
    bool Send(const NetworkMessage &message) override;

    /**
     * Pulls a data from the socket and writes it into a NetworkMessage.
     * 
     * @param message Reference to the NetworkMessage object to write data to
     * @return false if there is no packet to be received, otherwise true.
     */
    bool Receive(NetworkMessage &message) override;

private:
    zmq::context_t context;
    zmq::socket_t socket;
    std::vector<std::string> peers;
};

} // namespace svanes
