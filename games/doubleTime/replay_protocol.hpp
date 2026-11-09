#pragma once

#include <svanes/network/message_serialization.hpp>

#include <optional>
#include <stdexcept>

enum class DoubleTimeReplayMessage : std::uint8_t {
    Record,
    Save,
    Play,
    Exit,
    Status,
    Snapshot
};

inline constexpr std::uint32_t kDoubleTimeReplayMagic = 0x44545250;

inline svanes::MessageWriter
DoubleTimeReplayWriter(DoubleTimeReplayMessage kind) {
    svanes::MessageWriter writer;
    writer.WriteUint32(kDoubleTimeReplayMagic);
    writer.WriteUint8(static_cast<std::uint8_t>(kind));
    return writer;
}

inline std::optional<DoubleTimeReplayMessage>
DoubleTimeReplayKind(const svanes::NetworkMessage &message) {
    if (message.bytes.size() < 4) {
        return std::nullopt;
    }
    svanes::MessageReader reader(message);
    if (reader.ReadUint32() != kDoubleTimeReplayMagic) {
        return std::nullopt;
    }
    const auto kind = reader.ReadUint8();
    if (kind > static_cast<std::uint8_t>(DoubleTimeReplayMessage::Snapshot)) {
        throw std::invalid_argument("Unknown Double Time replay message.");
    }
    return static_cast<DoubleTimeReplayMessage>(kind);
}
