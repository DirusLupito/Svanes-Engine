#include <svanes/network/udp_msg_pipe.hpp>

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string_view>

namespace svanes {

UdpMsgPipe::UdpMsgPipe(std::uint16_t local_port)
    // Binds the local port to a dgram socket.
    // dgram is used to enable UDP
    : socket(context, zmq::socket_type::dgram) {
    socket.set(zmq::sockopt::linger, 0);
    socket.bind(MakeUdpEndpoint("*", local_port));
}

UdpMsgPipe::UdpMsgPipe(std::uint16_t local_port, const std::string &remote_host,
                       std::uint16_t remote_port)
    : UdpMsgPipe(local_port) {
    AddRemote(remote_host, remote_port);
}

ConnectionId UdpMsgPipe::AddRemote(const std::string &host, std::uint16_t port) {
    if (port == 0) {
        throw std::invalid_argument("UdpMsgPipe: remote port must be nonzero.");
    }
    // Normalize each octet so configured and received addresses use the same text.
    std::string canonical;
    std::string_view remaining = host;
    for (std::uint32_t index = 0; index < 4; ++index) {
        const auto separator = remaining.find('.');
        const auto part = remaining.substr(0, separator);
        std::uint32_t octet = 0;
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), octet);
        if (part.empty() || parsed.ec != std::errc{} ||
            parsed.ptr != part.data() + part.size() || octet > 255 ||
            (index < 3 && separator == std::string_view::npos) ||
            (index == 3 && separator != std::string_view::npos)) {
            throw std::invalid_argument("UdpMsgPipe: expected a numeric IPv4 address: " + host);
        }
        if (index != 0) {
            canonical += '.';
        }
        canonical += std::to_string(octet);
        if (index < 3) {
            remaining.remove_prefix(separator + 1);
        }
    }
    return RememberConnection(canonical + ":" + std::to_string(port));
}

bool UdpMsgPipe::Send(ConnectionId destination, const NetworkMessage &message) {
    const auto &route = Route(destination);
    if (message.bytes.size() > 65507) {
        throw std::length_error("UdpMsgPipe: payload exceeds the IPv4 UDP datagram limit.");
    }
    // ZeroMQ expects the destination address followed by the datagram payload.
    if (!socket.send(zmq::buffer(route),
                     zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
        return false;
    }
    if (!socket.send(zmq::buffer(message.bytes.data(), message.bytes.size()),
                     zmq::send_flags::dontwait)) {
        throw std::runtime_error("UdpMsgPipe: could not complete a datagram send.");
    }
    return true;
}

bool UdpMsgPipe::Receive(ReceivedMessage &received) {
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

    // ZeroMQ includes a terminating null byte that configured addresses lack.
    std::string route = address.to_string();
    if (!route.empty() && route.back() == '\0') {
        route.pop_back();
    }
    received.source = RememberConnection(route);
    // Reads the body into the message reference
    const auto *bytes = body.data<std::byte>();
    received.message =
        NetworkMessage{std::vector<std::byte>(bytes, bytes + body.size())};
    return true;
}

} // namespace svanes
