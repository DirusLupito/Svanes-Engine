#include <svanes/network/udp_msg_pipe.hpp>

#include <algorithm>
#include <stdexcept>

namespace svanes {

UdpMsgPipe::UdpMsgPipe(std::uint16_t local_port)
    // Binds the local port to a dgram socket.
    // dgram is used to enable UDP
    : socket(context, zmq::socket_type::dgram) {
    socket.bind(MakeUdpEndpoint("*", local_port));
}

UdpMsgPipe::UdpMsgPipe(std::uint16_t local_port, const std::string &remote_host,
                       std::uint16_t remote_port)
    : UdpMsgPipe(local_port) { // Calls the other constructor to create the pipe 
    // Checks to make sure the remote host has a correctly formatted address
    if (remote_host.find_first_not_of("0123456789.") != std::string::npos) {
        throw std::invalid_argument("UdpMsgPipe: remote host '" + remote_host +
                                    "' must be a numeric IPv4 address.");
    }
    // Stores the remote peer's address for later use in sending 
    peers.push_back(remote_host + ":" + std::to_string(remote_port));
}

bool UdpMsgPipe::Send(const NetworkMessage &message) {
    if (peers.empty()) { // If there are no peers stored, the message cannot be sent
        return false;
    }

    // Loop through all peers for this pipe.
    // This allows us to broadcast to multiple clients from a server,
    // or to send to a single peer if the pipe only has one
    for (const std::string &peer : peers) { 
        // First send the peer address to specify this messages destination
        const zmq::send_result_t address_sent =
            socket.send(zmq::buffer(peer),
                        zmq::send_flags::sndmore | zmq::send_flags::dontwait);
        if (!address_sent.has_value()) {
            continue;
        }
        // Then send the actual message content
        socket.send(zmq::buffer(message.bytes.data(), message.bytes.size()),
                    zmq::send_flags::none);
    }
    // The dgram socket, necessary for UDP transmission, doesn't establish a persistent connection.
    // As such, the address must be included with every message so that the socket knows where
    // to send it.

    return true;
}

bool UdpMsgPipe::Receive(NetworkMessage &message) {
    // Check if there is a packet waiting, if not returns false.
    // If there is, pulls the address frame from the socket
    zmq::message_t address;
    if (!socket.recv(address, zmq::recv_flags::dontwait).has_value()) {
        return false;
    }

    // Pulls the message content from the socket
    zmq::message_t body;
    if (!socket.recv(body, zmq::recv_flags::none).has_value()) {
        throw std::runtime_error(
            "UdpMsgPipe: received an address frame without a body frame.");
    }

    // If this is the first message from this address, adds it to the list of peers
    const std::string peer = address.to_string();
    if (std::find(peers.begin(), peers.end(), peer) == peers.end()) {
        peers.push_back(peer);
    }

    // Reads the body into the message reference
    const auto *bytes = body.data<std::byte>();
    message =
        NetworkMessage{std::vector<std::byte>(bytes, bytes + body.size())};
    return true;
}

} // namespace svanes
