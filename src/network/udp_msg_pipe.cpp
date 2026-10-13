#include <svanes/network/udp_msg_pipe.hpp>

#include <charconv>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace svanes {

void UdpMsgPipe::Listen(const std::string &address) {
    if (socket) {
        throw std::logic_error(
            "UdpMsgPipe: close the pipe before configuring it again.");
    }

    zmq::socket_t receiver(context, zmq::socket_type::dgram);

    // Bind the socket to the specified address to establish the receiving
    // direction.
    receiver.bind("udp://" + address);

    // Now the pipe owns the socket and is ready to receive datagrams.
    socket = std::move(receiver);
}

void UdpMsgPipe::Connect(const std::string &address) {
    if (socket) {
        throw std::logic_error(
            "UdpMsgPipe: close the pipe before configuring it again.");
    }

    const std::string_view endpoint = address;

    // Assuming the address is in the format IPv4:PORT...

    const auto colon = endpoint.rfind(':');
    if (colon == std::string_view::npos) {
        throw std::invalid_argument(
            "UdpMsgPipe: expected a destination in IPv4:PORT form.");
    }

    // ...we read everything after the colon as a decimal port number...
    const auto port_text = endpoint.substr(colon + 1);
    std::uint32_t port = 0;
    const auto parsed_port = std::from_chars(
        port_text.data(), port_text.data() + port_text.size(), port);

    // Valid ports are from 1 to 65535.
    if (port_text.empty() || parsed_port.ec != std::errc{} ||
        parsed_port.ptr != port_text.data() + port_text.size() || port == 0 ||
        port > 65535) {
        throw std::invalid_argument(
            "UdpMsgPipe: destination port must be from 1 to 65535.");
    }

    // The normalized address we build from the parsed numbers.
    // For example, 127.000.000.001 becomes 127.0.0.1.
    std::string canonical;

    // ... and everything before the colon is the IPv4 address. Each iteration
    // consumes one of its four octets, leaving the rest for the next iteration.
    std::string_view remaining = endpoint.substr(0, colon);
    for (std::uint32_t index = 0; index < 4; ++index) {
        // Take the next dot-separated part. For the last octet, there should
        // be no dot, so part contains all of the remaining address.
        const auto separator = remaining.find('.');
        const auto part = remaining.substr(0, separator);

        // Read this part as a decimal octet.
        std::uint32_t octet = 0;
        const auto parsed =
            std::from_chars(part.data(), part.data() + part.size(), octet);

        // Each octet must be a complete number from 0 to 255. The first three
        // must be followed by a dot, and the fourth must be the last part.
        if (part.empty() || parsed.ec != std::errc{} ||
            parsed.ptr != part.data() + part.size() || octet > 255 ||
            (index < 3 && separator == std::string_view::npos) ||
            (index == 3 && separator != std::string_view::npos)) {
            throw std::invalid_argument(
                "UdpMsgPipe: expected a numeric IPv4 destination: " + address);
        }

        // Put dots between the normalized octets, but not before the first.
        if (index != 0) {
            canonical += '.';
        }

        // Convert the parsed number back to decimal text without leading zeros.
        canonical += std::to_string(octet);

        // Move past this octet and its dot to read the next one. There is
        // nothing left to consume after the fourth octet.
        if (index < 3) {
            remaining.remove_prefix(separator + 1);
        }
    }

    // ZMQ_DGRAM does not support connect, so the destination is stored locally
    // and the sending socket is bound to an available local port.

    // Append the port to the normalized IPv4 address, giving IPv4:PORT.
    canonical += ':' + std::to_string(port);

    // Create a UDP datagram socket using this pipe's ZeroMQ context.
    zmq::socket_t sender(context, zmq::socket_type::dgram);

    // When this socket closes, discard any messages still waiting to be sent.
    sender.set(zmq::sockopt::linger, 0);

    // DGRAM needs a local binding before it can send. But we don't care
    // which local port is used, so we just let the OS pick one for us.
    sender.bind("udp://*:0");

    // This message pipe owns the socket now.
    socket = std::move(sender);

    // Store the destination to supply as the address frame for each Send.
    destination = std::move(canonical);
}

bool UdpMsgPipe::Send(const NetworkMessage &message) {
    if (!socket || destination.empty()) {
        throw std::logic_error(
            "UdpMsgPipe: Send requires a connected sending pipe.");
    }

    // Check if the message exceeds the ZeroMQ DGRAM UDP buffer limit of 8192
    // bytes.
    if (message.bytes.size() > 8192) {
        throw std::length_error("UdpMsgPipe: payload exceeds the ZeroMQ UDP "
                                "buffer limit of 8192 bytes.");
    }

    // First, we send the destination frame telling the socket where to send the
    // datagram. (sndmore tells the socket that more frames are coming, so it
    // should not send the datagram yet.)
    if (!socket.send(zmq::buffer(destination),
                     zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
        return false;
    }

    // Next, we send the datagram body frame containing the actual message
    // bytes, without blocking (dontwait). If the socket's internal buffers are
    // full, this will return false.
    if (!socket.send(zmq::buffer(message.bytes.data(), message.bytes.size()),
                     zmq::send_flags::dontwait)) {
        throw std::runtime_error(
            "UdpMsgPipe: could not complete a datagram send.");
    }

    return true;
}

bool UdpMsgPipe::Receive(NetworkMessage &message) {
    if (!socket || !destination.empty()) {
        throw std::logic_error(
            "UdpMsgPipe: Receive requires a listening receiving pipe.");
    }

    zmq::message_t address;

    // We delegate to the socket's recv method, which will return an empty
    // optional if no message is available. dontwait so we don't block if no
    // message is available.
    if (!socket.recv(address, zmq::recv_flags::dontwait).has_value()) {
        return false;
    }

    // So if we're here there is a message available, but we need to receive the
    // body frame as well.

    zmq::message_t body;

    // Every message should have an address frame followed by a body frame
    // containing the actual datagram. If we receive an address frame but no
    // body frame, that's an error.
    if (!socket.recv(body, zmq::recv_flags::none).has_value()) {
        throw std::runtime_error(
            "UdpMsgPipe: received an address frame without a body frame.");
    }

    // Copy the received bytes into the provided NetworkMessage object.

    const auto *bytes = body.data<std::byte>();
    message =
        NetworkMessage{std::vector<std::byte>(bytes, bytes + body.size())};

    return true;
}

void UdpMsgPipe::Close() {
    socket.close();
    destination.clear();
}

} // namespace svanes
