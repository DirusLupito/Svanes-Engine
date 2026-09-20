#include <svanes/network/message_serialization.hpp>

#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>

namespace svanes {

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

void MessageWriter::WriteUnsigned(std::uint64_t value, std::uint32_t width) {
    // Extract bytes explicitly instead of copying the machine's integer layout.
    for (std::uint32_t remaining = width; remaining > 0; --remaining) {
        message.bytes.push_back(static_cast<std::byte>(
            (value >> ((remaining - 1) * 8)) & 0xff));
    }
}

void MessageWriter::WriteUint8(std::uint8_t value) { WriteUnsigned(value, 1); }
void MessageWriter::WriteUint16(std::uint16_t value) { WriteUnsigned(value, 2); }
void MessageWriter::WriteUint32(std::uint32_t value) { WriteUnsigned(value, 4); }
void MessageWriter::WriteUint64(std::uint64_t value) { WriteUnsigned(value, 8); }

void MessageWriter::WriteInt32(std::int32_t value) {
    WriteUint32(std::bit_cast<std::uint32_t>(value));
}

void MessageWriter::WriteFloat32(float value) {
    // Preserve the float's bits instead of converting its numeric value.
    WriteUint32(std::bit_cast<std::uint32_t>(value));
}

void MessageWriter::WriteBool(bool value) { WriteUint8(value ? 1 : 0); }

void MessageWriter::WriteBytes(std::span<const std::byte> bytes) {
    message.bytes.insert(message.bytes.end(), bytes.begin(), bytes.end());
}

NetworkMessage MessageWriter::Finish() {
    return std::exchange(message, NetworkMessage{});
}

MessageReader::MessageReader(const NetworkMessage &message)
    : bytes(message.bytes) {}

MessageReader::MessageReader(std::span<const std::byte> bytes) : bytes(bytes) {}

std::span<const std::byte> MessageReader::ReadBytes(std::size_t count) {
    // Check the whole field before moving the cursor.
    if (count > Remaining()) {
        throw std::invalid_argument("MessageReader: truncated message.");
    }
    const auto result = bytes.subspan(cursor, count);
    cursor += count;
    return result;
}

std::uint64_t MessageReader::ReadUnsigned(std::uint32_t width) {
    std::uint64_t value = 0;
    for (const std::byte byte : ReadBytes(width)) {
        value = (value << 8) | std::to_integer<std::uint8_t>(byte);
    }
    return value;
}

std::uint8_t MessageReader::ReadUint8() {
    return static_cast<std::uint8_t>(ReadUnsigned(1));
}

std::uint16_t MessageReader::ReadUint16() {
    return static_cast<std::uint16_t>(ReadUnsigned(2));
}

std::uint32_t MessageReader::ReadUint32() {
    return static_cast<std::uint32_t>(ReadUnsigned(4));
}

std::uint64_t MessageReader::ReadUint64() { return ReadUnsigned(8); }

std::int32_t MessageReader::ReadInt32() {
    return std::bit_cast<std::int32_t>(ReadUint32());
}

float MessageReader::ReadFloat32() {
    return std::bit_cast<float>(ReadUint32());
}

bool MessageReader::ReadBool() {
    const auto value = ReadUint8();
    if (value > 1) {
        throw std::invalid_argument("MessageReader: boolean must be 0 or 1.");
    }
    return value != 0;
}

std::size_t MessageReader::Remaining() const { return bytes.size() - cursor; }

}
