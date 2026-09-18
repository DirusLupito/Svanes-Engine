#include <svanes/network/udp_msg_pipe.hpp>

#include <algorithm>
#include <stdexcept>

namespace svanes {

UdpMsgPipe::UdpMsgPipe(std::uint16_t local_port)
    : socket(context, zmq::socket_type::dgram) {
    socket.bind(MakeUdpEndpoint("*", local_port));
}

UdpMsgPipe::UdpMsgPipe(std::uint16_t local_port, const std::string &remote_host,
                       std::uint16_t remote_port)
    : UdpMsgPipe(local_port) {
    if (remote_host.find_first_not_of("0123456789.") != std::string::npos) {
        throw std::invalid_argument("UdpMsgPipe: remote host '" + remote_host +
                                    "' must be a numeric IPv4 address.");
    }

    peers.push_back(remote_host + ":" + std::to_string(remote_port));
}

bool UdpMsgPipe::Send(const NetworkMessage &message) {
    if (peers.empty()) {
        return false;
    }

    for (const std::string &peer : peers) {
        const zmq::send_result_t address_sent =
            socket.send(zmq::buffer(peer),
                        zmq::send_flags::sndmore | zmq::send_flags::dontwait);
        if (!address_sent.has_value()) {
            continue;
        }

        socket.send(zmq::buffer(message.bytes.data(), message.bytes.size()),
                    zmq::send_flags::none);
    }

    return true;
}

bool UdpMsgPipe::Receive(NetworkMessage &message) {
    zmq::message_t address;
    if (!socket.recv(address, zmq::recv_flags::dontwait).has_value()) {
        return false;
    }

    zmq::message_t body;
    if (!socket.recv(body, zmq::recv_flags::none).has_value()) {
        throw std::runtime_error(
            "UdpMsgPipe: received an address frame without a body frame.");
    }

    const std::string peer = address.to_string();
    if (std::find(peers.begin(), peers.end(), peer) == peers.end()) {
        peers.push_back(peer);
    }

    const auto *bytes = body.data<std::byte>();
    message =
        NetworkMessage{std::vector<std::byte>(bytes, bytes + body.size())};
    return true;
}

} // namespace svanes
