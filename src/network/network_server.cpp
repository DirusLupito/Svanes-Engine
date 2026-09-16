
#include <svanes/network/network_server.hpp>

#include "network_internal.hpp"

namespace svanes {

NetworkServer::NetworkServer(std::uint16_t broadcast_port, std::uint16_t inbound_port)
    : broadcast_socket(context, zmq::socket_type::pub)
    , inbound_socket(context, zmq::socket_type::pull) {
    broadcast_socket.bind(MakeTcpEndpoint("*", broadcast_port));
    inbound_socket.bind(MakeTcpEndpoint("*", inbound_port));
}

void NetworkServer::SendRaw(const void *data, std::size_t size) {
    broadcast_socket.send(zmq::buffer(data, size), zmq::send_flags::dontwait);
}

std::vector<NetworkMessage> NetworkServer::PollInbound() {
    return internal::NetworkInternal::DrainMessages(inbound_socket);
}

} // namespace svanes
