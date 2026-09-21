#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace svanes {

/**
 * Concrete implementation of a MsgPipe using TCP protocol.
 * Uses ZMQ_ROUTER and ZMQ_DEALER socket types
 */
class TcpMsgPipe final : public MsgPipe {
public:
    /**
     * Creates a router socket bound to the local port.
     * 
     * @param local_port The port number to bind the socket to.
     */
    explicit TcpMsgPipe(std::uint16_t local_port);

    /**
     * Creates a dealer socket and connects to a remote host.
     * 
     * @param remote_host The hostname of the remote peer the socket will be bound to.
     * @param remote_port The port number of the remote peer the socket will be bound to.
     */
    TcpMsgPipe(const std::string &remote_host, std::uint16_t remote_port);

    /**
     * Servers send a message and address through a socket to all peers.
     * Clients send the message body directly
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
    bool listening;
    std::vector<std::string> peers;
};

} // namespace svanes
