#include <svanes/network/tcp_msg_pipe.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace svanes {

TcpMsgPipe::TcpMsgPipe(std::uint16_t local_port)
    : socket(context, zmq::socket_type::router), listening(true) {
    // Binds the socket to the given local port
    socket.set(zmq::sockopt::linger, 0);
    socket.set(zmq::sockopt::router_mandatory, 1);
    socket.bind(MakeTcpEndpoint("*", local_port));
}

TcpMsgPipe::TcpMsgPipe(const std::string &remote_host,
                       std::uint16_t remote_port)
    : socket(context, zmq::socket_type::dealer), listening(false) {
    socket.set(zmq::sockopt::linger, 0);
    // Connects the socket to the given remote host
    socket.connect(MakeTcpEndpoint(remote_host, remote_port));
    RememberConnection(MakeTcpEndpoint(remote_host, remote_port));
}

bool TcpMsgPipe::Send(ConnectionId destination, const NetworkMessage &message) {
    const auto &route = Route(destination);
    const zmq::const_buffer body =
        zmq::buffer(message.bytes.data(), message.bytes.size());

    // If this is a client pipe, send and return whether or not it was accepted
    if (!listening) {
        return socket.send(body, zmq::send_flags::dontwait).has_value();
    }

    // A listener prefixes the payload with the client's ZeroMQ routing identity.
    if (!socket.send(zmq::buffer(route),
                     zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
        return false;
    }
    if (!socket.send(body, zmq::send_flags::dontwait)) {
        throw std::runtime_error("TcpMsgPipe: could not complete a routed send.");
    }
    return true;
}

bool TcpMsgPipe::Receive(ReceivedMessage &received) {
    zmq::message_t frame;
    if (!socket.recv(frame, zmq::recv_flags::dontwait).has_value()) {
        return false;
    }

    // If this is a server pipe, get peer information and pull the message body
    if (listening) {
        if (!frame.more()) {
            throw std::runtime_error("TcpMsgPipe: routing identity has no body.");
        }
        received.source = RememberConnection(frame.to_string());

        zmq::message_t body;
        if (!socket.recv(body, zmq::recv_flags::none).has_value()) {
            throw std::runtime_error(
                "TcpMsgPipe: received an identity frame without a body frame.");
        }
        frame = std::move(body);
    } else {
        // A client registers its only connection, the server, during construction.
        received.source = ConnectionId{1};
    }
    if (frame.more()) {
        // Drain the rejected message so the next receive starts at a new message.
        while (frame.more()) {
            if (!socket.recv(frame, zmq::recv_flags::dontwait)) {
                throw std::runtime_error("TcpMsgPipe: incomplete multipart message.");
            }
        }
        throw std::runtime_error("TcpMsgPipe: expected exactly one payload frame.");
    }

    // Read message content into the provided reference.
    const auto *bytes = frame.data<std::byte>();
    received.message =
        NetworkMessage{std::vector<std::byte>(bytes, bytes + frame.size())};
    return true;
}

} // namespace svanes
