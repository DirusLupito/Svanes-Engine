#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace svanes {

/**
 * The ID of the client that is associated with this message
 */
using ClientId = std::uint32_t;

/**
 * Generates IDs for game clients to use in communication.
 */
ClientId GenerateClientId();

/**
 * Builds URI string for 0MQ TCP
 */
std::string MakeTcpEndpoint(const std::string &host, std::uint16_t port);

std::string MakeUdpEndpoint(const std::string &host, std::uint16_t port);

/**
 * Representation of an arbitrary message being sent over the network.
 * All messages are sent as raw bytes
 */
struct NetworkMessage {
    std::vector<std::byte> bytes;

    /**
     * Copies this message into memory for interpretation.
     * Checks to make sure that the type is copyable, as well as
     * making sure that the size of the message as specified by
     * the sender matches the actual size of the bytes.
     */
    template <typename T> T As() const {
        static_assert(std::is_trivially_copyable_v<T>,
                      "NetworkMessage::As requires a trivially copyable type.");

        if (bytes.size() != sizeof(T)) {
            throw std::runtime_error(
                "NetworkMessage::As: message size " + std::to_string(bytes.size()) +
                " does not match sizeof(T) " + std::to_string(sizeof(T)) + "."
            );
        }

        T value;
        std::memcpy(&value, bytes.data(), sizeof(T));
        return value;
    }
};

} // namespace svanes
