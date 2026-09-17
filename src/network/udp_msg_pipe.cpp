#include <svanes/network/udp_msg_pipe.hpp>

#include <cstring>
#include <vector>

namespace svanes {

namespace {
constexpr const char *kDataGroup = "msg";
constexpr const char *kEndOfStreamGroup = "eof";
} // namespace

UdpMsgPipe::UdpMsgPipe(std::uint16_t local_port, const std::string &remote_host,
                       std::uint16_t remote_port)
    : radio_socket(context, zmq::socket_type::radio),
      dish_socket(context, zmq::socket_type::dish) {
    radio_socket.connect(MakeUdpEndpoint(remote_host, remote_port));
    dish_socket.bind(MakeUdpEndpoint("*", local_port));
    dish_socket.join(kDataGroup);
    dish_socket.join(kEndOfStreamGroup);
}

bool UdpMsgPipe::Send(const NetworkMessage &message) {
    if (local_misuse_failed) {
        return false;
    }

    if (send_closed) {
        local_misuse_failed = true;
        return false;
    }

    zmq::message_t outgoing(message.bytes.data(), message.bytes.size());
    outgoing.set_group(kDataGroup);
    const zmq::send_result_t result =
        radio_socket.send(std::move(outgoing), zmq::send_flags::dontwait);
    return result.has_value();
}

bool UdpMsgPipe::Receive(NetworkMessage &message) {
    zmq::message_t received;
    const zmq::recv_result_t result =
        dish_socket.recv(received, zmq::recv_flags::dontwait);
    if (!result.has_value()) {
        return false;
    }

    if (std::strcmp(received.group(), kEndOfStreamGroup) == 0) {
        remote_end_of_stream = true;
        return false;
    }

    const auto *bytes = static_cast<const std::byte *>(received.data());
    message =
        NetworkMessage{std::vector<std::byte>(bytes, bytes + received.size())};
    return true;
}

void UdpMsgPipe::SendEndOfStream() {
    if (send_closed) {
        return;
    }

    send_closed = true;
    zmq::message_t marker(static_cast<std::size_t>(0));
    marker.set_group(kEndOfStreamGroup);
    radio_socket.send(std::move(marker), zmq::send_flags::dontwait);
}

bool UdpMsgPipe::AtEndOfStream() const { return remote_end_of_stream; }

bool UdpMsgPipe::Failed() const { return local_misuse_failed; }

} // namespace svanes
