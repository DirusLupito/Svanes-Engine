#include <svanes/network/tcp_msg_pipe.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace svanes {

TcpMsgPipe::TcpMsgPipe(std::uint16_t local_port)
    : socket(context, zmq::socket_type::router), listening(true) {
    // Binds the socket to the given local port
    socket.set(zmq::sockopt::linger, 0);
    socket.bind(MakeTcpEndpoint("*", local_port));
}

TcpMsgPipe::TcpMsgPipe(const std::string &remote_host,
                       std::uint16_t remote_port)
    : socket(context, zmq::socket_type::dealer), listening(false) {
    socket.set(zmq::sockopt::linger, 0);
    // Connects the socket to the given remote host
    socket.connect(MakeTcpEndpoint(remote_host, remote_port));
}

bool TcpMsgPipe::Send(const NetworkMessage &message) {
    const zmq::const_buffer body =
        zmq::buffer(message.bytes.data(), message.bytes.size());

    // If this is a client pipe, send and return whether or not it was accepted
    if (!listening) {
        return socket.send(body, zmq::send_flags::dontwait).has_value();
    }
    // If this is a server pipe, broadcast to all peers
    if (peers.empty()) {
        return false;
    }

    // Loop through peers and send the message content to each of them.
    for (const std::string &peer : peers) {
        const zmq::send_result_t address_sent =
            socket.send(zmq::buffer(peer),
                        zmq::send_flags::sndmore | zmq::send_flags::dontwait);
        if (!address_sent.has_value()) {
            continue;
        }

        socket.send(body, zmq::send_flags::none);
    }

    return true;
}

bool TcpMsgPipe::Receive(NetworkMessage &message) {
    zmq::message_t frame;
    if (!socket.recv(frame, zmq::recv_flags::dontwait).has_value()) {
        return false;
    }

    // If this is a server pipe, get peer information and pull the message body
    if (listening) {
        const std::string peer = frame.to_string();
        if (std::find(peers.begin(), peers.end(), peer) == peers.end()) {
            peers.push_back(peer);
        }

        zmq::message_t body;
        if (!socket.recv(body, zmq::recv_flags::none).has_value()) {
            throw std::runtime_error(
                "TcpMsgPipe: received an identity frame without a body frame.");
        }
        frame = std::move(body);
    }

    // Read message content into the provided reference.
    const auto *bytes = frame.data<std::byte>();
    message =
        NetworkMessage{std::vector<std::byte>(bytes, bytes + frame.size())};
    return true;
}

} // namespace svanes
