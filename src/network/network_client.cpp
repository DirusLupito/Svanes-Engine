#include <svanes/network/network_client.hpp>

#include "network_internal.hpp"

namespace svanes {

NetworkClient::NetworkClient(const std::string &host, std::uint16_t broadcast_port, std::uint16_t outbound_port)
    : id(GenerateClientId())
    , broadcast_socket(context, zmq::socket_type::sub)
    , outbound_socket(context, zmq::socket_type::push) {
    broadcast_socket.set(zmq::sockopt::subscribe, "");
    broadcast_socket.connect(MakeTcpEndpoint(host, broadcast_port));
    outbound_socket.connect(MakeTcpEndpoint(host, outbound_port));
}

ClientId NetworkClient::Id() const { return id; }

void NetworkClient::SendRaw(const void *data, std::size_t size) {
    outbound_socket.send(zmq::buffer(data, size), zmq::send_flags::dontwait);
}

std::vector<NetworkMessage> NetworkClient::PollBroadcast() {
    return internal::NetworkInternal::DrainMessages(broadcast_socket);
}

} // namespace svanes
