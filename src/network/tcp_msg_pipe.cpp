#include <svanes/network/tcp_msg_pipe.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace svanes {

namespace {

constexpr ConnectionId kConnection{1};

} // namespace

TcpMsgPipe::TcpMsgPipe(std::uint16_t local_port)
    // Binds the local port to the pair socket
    : socket(context, zmq::socket_type::pair), listening(true) {
    socket.set(zmq::sockopt::linger, 0);
    socket.bind(MakeTcpEndpoint("*", local_port));
    RememberConnection("pair");
}

TcpMsgPipe::TcpMsgPipe(const std::string &remote_host,
                       std::uint16_t remote_port)
    : socket(context, zmq::socket_type::pair), listening(false) {
    socket.set(zmq::sockopt::linger, 0);
    socket.connect(MakeTcpEndpoint(remote_host, remote_port));
    RememberConnection(MakeTcpEndpoint(remote_host, remote_port));
}

bool TcpMsgPipe::Send(ConnectionId destination, const NetworkMessage &message) {
    // Check for incorrect destination
    if (destination != kConnection) {
        throw std::invalid_argument("TcpMsgPipe: unknown connection.");
    }
    return socket.send(zmq::buffer(message.bytes.data(), message.bytes.size()),
                       zmq::send_flags::dontwait)
        .has_value();
}

bool TcpMsgPipe::Receive(ReceivedMessage &received) {
    zmq::message_t body;
    // If no message value, return false to indicate message failure
    if (!socket.recv(body, zmq::recv_flags::dontwait).has_value()) {
        return false;
    }

    const auto *bytes = body.data<std::byte>();
    received.source = kConnection;
    received.message =
        NetworkMessage{std::vector<std::byte>(bytes, bytes + body.size())};
    return true;
}

} // namespace svanes
