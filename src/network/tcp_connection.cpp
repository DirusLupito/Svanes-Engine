#include <svanes/network/tcp_connection.hpp>

#include <svanes/network/message_serialization.hpp>
#include <svanes/network/network_server.hpp>
#include <svanes/network/tcp_msg_pipe.hpp>

#include <stdexcept>
#include <utility>

namespace svanes {

TcpClientConnector::TcpClientConnector(std::string host,
                                       std::uint16_t joining_port)
    : host(std::move(host)), socket(context, zmq::socket_type::req) {
    socket.set(zmq::sockopt::linger, 0);
    socket.connect(MakeTcpEndpoint(this->host, joining_port));
    if (!socket.send(zmq::message_t{}, zmq::send_flags::dontwait)) {
        throw std::runtime_error("Could not send TCP join request.");
    }
}

std::optional<TcpConnection> TcpClientConnector::Poll() {
    zmq::message_t reply;
    if (!socket.recv(reply, zmq::recv_flags::dontwait)) {
        return std::nullopt;
    }

    const auto *bytes = reply.data<std::byte>();
    MessageReader reader(std::span<const std::byte>{bytes, reply.size()});
    const auto id = reader.ReadUint32();
    const auto port = reader.ReadUint16();
    if (id == 0 || port == 0) {
        throw std::invalid_argument("Invalid TCP connection assignment.");
    }

    return TcpConnection{id, std::make_unique<TcpMsgPipe>(host, port)};
}

TcpServerListener::TcpServerListener(std::uint16_t joining_port)
    : socket(context, zmq::socket_type::rep) {
    socket.set(zmq::sockopt::linger, 0);
    socket.bind(MakeTcpEndpoint("*", joining_port));
}

void TcpServerListener::Poll(NetworkServer &server) {
    zmq::message_t request;
    while (socket.recv(request, zmq::recv_flags::dontwait)) {

        // Each client needs its own pair connection. Let the system choose
        // an available port so clients can share the joining address.
        std::uint16_t port = 0;
        const auto session = server.AddSession([&port] {
            auto pipe = std::make_unique<TcpMsgPipe>(std::uint16_t{0});
            port = pipe->Port();
            return pipe;
        });

        MessageWriter writer;
        writer.WriteUint32(static_cast<ClientId>(session + 1));
        writer.WriteUint16(port);
        const auto reply = writer.Finish();
        if (!socket.send(zmq::buffer(reply.bytes.data(), reply.bytes.size()),
                         zmq::send_flags::dontwait)) {
            throw std::runtime_error(
                "Could not send TCP connection assignment.");
        }
    }
}

} // namespace svanes
