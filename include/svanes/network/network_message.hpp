#pragma once

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace svanes {

/**
 * Representation of an arbitrary message being sent over the network.
 * All messages are sent as raw bytes
 */
class NetworkMessage {
public:
    /**
     * Constructs an empty message.
     */
    NetworkMessage();

    /**
     * Constructs a message owning the provided bytes.
     * @param bytes The bytes to store in the message.
     */
    explicit NetworkMessage(std::vector<std::byte> bytes);

    std::vector<std::byte> bytes;

    template <typename T> static NetworkMessage From(const T &value) {
        static_assert(
            std::is_trivially_copyable_v<T>,
            "NetworkMessage::From requires a trivially copyable type.");

        NetworkMessage message;
        message.bytes.resize(sizeof(T));
        std::memcpy(message.bytes.data(), &value, sizeof(T));
        return message;
    }

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
            throw std::runtime_error("NetworkMessage::As: message size " +
                                     std::to_string(bytes.size()) +
                                     " does not match sizeof(T) " +
                                     std::to_string(sizeof(T)) + ".");
        }

        T value;
        std::memcpy(&value, bytes.data(), sizeof(T));
        return value;
    }
};

} // namespace svanes
