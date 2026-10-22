#include <svanes/network/udp_msg_pipe.hpp>

#include <svanes/network/message_serialization.hpp>

#include <algorithm>
#include <charconv>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace svanes {

// The largest datagram supported by ZeroMQ's fixed UDP buffers.
constexpr std::uint32_t DatagramSize = 8192;

// Each datagram starts with an 8-byte message ID, a 4-byte fragment index,
// and a 4-byte complete message size, all in big-endian order.
constexpr std::uint32_t FragmentHeaderSize = 16;

// The message bytes that fit after a datagram's fragment header.
constexpr std::uint32_t FragmentPayloadSize = DatagramSize - FragmentHeaderSize;

// The time allowed to assemble a message after its first fragment arrives.
constexpr auto FragmentLifetime = std::chrono::seconds(1);

/**
 * Validates a numeric IPv4:PORT address and normalizes it for ZMQ_DGRAM.
 *
 * @param address The address to validate.
 * @return The normalized IPv4:PORT address.
 * @throws std::invalid_argument if the IPv4 address is invalid or the port is
 * outside the range from 1 to 65535.
 */
static std::string CanonicalAddress(const std::string &address) {
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

    // Append the port to the normalized IPv4 address, giving IPv4:PORT.
    canonical += ':' + std::to_string(port);

    return canonical;
}

void UdpMsgPipe::Listen(const std::string &address) {
    if (socket) {
        throw std::logic_error(
            "UdpMsgPipe: close the pipe before configuring it again.");
    }
    zmq::socket_t new_socket(context, zmq::socket_type::dgram);

    // When this socket closes, discard any messages still waiting to be sent.
    new_socket.set(zmq::sockopt::linger, 0);

    // Bind the socket to the specified address.
    new_socket.bind("udp://" + address);

    // Now the pipe owns the socket and is ready to send and receive datagrams.
    socket = std::move(new_socket);
}

bool UdpMsgPipe::Send(const std::string &address,
                      const NetworkMessage &message) {
    if (!socket) {
        throw std::logic_error("UdpMsgPipe: Send requires a listening pipe.");
    }

    // ZMQ_DGRAM requires a destination address with each send.
    const std::string destination = CanonicalAddress(address);

    if (message.bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error(
            "UdpMsgPipe: message size exceeds the fragment header's range.");
    }

    const auto size = static_cast<std::uint32_t>(message.bytes.size());
    const auto id = next_message_id++;
    const auto count = size == 0 ? 1 : 1 + (size - 1) / FragmentPayloadSize;
    const std::span<const std::byte> bytes = message.bytes;

    // For each fragment, send the following:
    // 8 bytes: message ID
    // 4 bytes: fragment index
    // 4 bytes: complete message size
    // 0 to 8176 bytes: fragment payload

    for (std::uint32_t index = 0; index < count; ++index) {
        const auto offset = index * FragmentPayloadSize;
        const auto length = std::min(FragmentPayloadSize, size - offset);
        MessageWriter writer;
        writer.WriteUint64(id);
        writer.WriteUint32(index);
        writer.WriteUint32(size);
        writer.WriteBytes(bytes.subspan(offset, length));
        const auto fragment = writer.Finish();

        // First, we send the destination frame telling the socket where to send
        // the datagram. sndmore tells ZeroMQ that the datagram's body frame
        // follows. dontwait returns false if the frame cannot be queued.
        if (!socket.send(zmq::buffer(destination),
                         zmq::send_flags::sndmore |
                             zmq::send_flags::dontwait)) {
            return false;
        }

        // Next, we send the datagram body frame containing the fragment header
        // and payload, without blocking (dontwait). If this send returns false,
        // we throw because the destination frame was already accepted but the
        // datagram could not be completed.
        if (!socket.send(
                zmq::buffer(fragment.bytes.data(), fragment.bytes.size()),
                zmq::send_flags::dontwait)) {
            throw std::runtime_error(
                "UdpMsgPipe: could not complete a datagram send.");
        }
    }

    return true;
}

bool UdpMsgPipe::Receive(std::string &address, NetworkMessage &message) {
    if (!socket) {
        throw std::logic_error(
            "UdpMsgPipe: Receive requires a listening pipe.");
    }

    // Expiration is based on the first fragment, so later fragments and
    // duplicates cannot keep an unfinished message alive indefinitely.
    const auto now = std::chrono::steady_clock::now();
    std::erase_if(incomplete, [&](const auto &entry) {
        return now - entry.second.started >= FragmentLifetime;
    });

    // Consume available fragments until a message is complete or the socket
    // has no more datagrams.
    while (true) {
        zmq::message_t source;
        if (!socket.recv(source, zmq::recv_flags::dontwait)) {
            return false;
        }

        // Every datagram has an address frame followed by its body frame.
        zmq::message_t body;
        if (!socket.recv(body, zmq::recv_flags::dontwait)) {
            throw std::runtime_error(
                "UdpMsgPipe: received an address frame without a body frame.");
        }

        // Read the fragment header and payload from the datagram body.
        // Remember, it's 8 bytes of message ID, 4 bytes of fragment index, 4
        // bytes of complete message size, and 0 to 8176 bytes of fragment
        // payload.
        MessageReader reader(
            std::span<const std::byte>{body.data<std::byte>(), body.size()});
        const auto id = reader.ReadUint64();
        const auto index = reader.ReadUint32();
        const auto size = reader.ReadUint32();
        const auto count = size == 0 ? 1 : 1 + (size - 1) / FragmentPayloadSize;

        // if the fragment index is out of range or the payload size does not
        // match the expected size for this fragment, the datagram is invalid.
        if (index >= count ||
            reader.Remaining() !=
                std::min(FragmentPayloadSize,
                         size - index * FragmentPayloadSize)) {
            throw std::invalid_argument(
                "UdpMsgPipe: invalid message fragment.");
        }

        // 0MQ writes the sender's address frame as NULL-terminated IPv4:PORT
        // text. Different senders might use the same message ID, so we need the
        // tuple (sender, message ID) to identify a unique message assembly.
        std::string sender = source.data<char>();
        auto [entry, inserted] = incomplete.try_emplace(
            std::pair{sender, id},
            IncompleteMessage{size, {}, std::chrono::steady_clock::now()});

        auto &assembly = entry->second;

        // All fragments belonging to the same incomplete message must agree
        // on the complete message size.
        if (!inserted && assembly.size != size) {
            throw std::invalid_argument(
                "UdpMsgPipe: inconsistent message size between fragments.");
        }

        // Keep each fragment once, in index order, without allocating the
        // complete message until all of its bytes have arrived.
        const auto payload = reader.ReadBytes(reader.Remaining());
        assembly.fragments.try_emplace(index, payload.begin(), payload.end());

        // If the number of fragments received is not equal to the expected
        // count, we cannot assemble the complete message yet, so we continue to
        // receive more fragments.
        if (assembly.fragments.size() != count) {
            continue;
        }

        // At this point, we have received all fragments for the message, so we
        // can assemble the complete message by concatenating the fragments in
        // order.

        // The complete message, to be returned to the caller.
        NetworkMessage complete;
        complete.bytes.reserve(size);

        // For each fragment in the assembly, in order of their indices, we
        // append their payloads to the complete message.
        for (const auto &[fragment_index, fragment] : assembly.fragments) {
            complete.bytes.insert(complete.bytes.end(), fragment.begin(),
                                  fragment.end());
        }

        incomplete.erase(entry);
        address = std::move(sender);
        message = std::move(complete);
        return true;
    }
}

void UdpMsgPipe::Close() {
    socket.close();
    incomplete.clear();
}

} // namespace svanes
