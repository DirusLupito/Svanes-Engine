#include "game_events.hpp"

#include <svanes/network/message_serialization.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

svanes::NetworkMessage DoubleTimeWorldSnapshotEvent::Write() const {
    if (objects.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Double Time snapshot is too large.");
    }
    svanes::MessageWriter writer;
    writer.WriteUint32(static_cast<std::uint32_t>(objects.size()));
    for (const auto &state : objects) {
        writer.WriteFloat32(state.transform.x);
        writer.WriteFloat32(state.transform.y);
        writer.WriteFloat32(state.transform.rotation);
        writer.WriteBool(state.movement_key_held);
        writer.WriteUint32(state.entity_id);
        writer.WriteUint8(static_cast<std::uint8_t>(state.kind));
        writer.WriteFloat32(state.width);
        writer.WriteFloat32(state.height);
        writer.WriteUint64(state.timeline_tics);
        writer.WriteBool(state.enabled);
        writer.WriteUint32(state.area_index);
        writer.WriteUint32(state.tilemap_index);
    }
    if (stats) {
        writer.WriteUint32(stats->deaths);
        writer.WriteUint32(stats->coins);
    }
    return writer.Finish();
}

std::unique_ptr<DoubleTimeWorldSnapshotEvent>
DoubleTimeWorldSnapshotEvent::Read(const svanes::NetworkMessage &message) {
    svanes::MessageReader reader(message);
    const auto count = reader.ReadUint32();
    if (count > reader.Remaining() / 43) {
        throw std::invalid_argument("Truncated Double Time snapshot.");
    }
    std::vector<DoubleTimeEntityState> states;
    states.reserve(count);
    std::uint32_t clocks = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        DoubleTimeEntityState state{};
        state.transform.x = reader.ReadFloat32();
        state.transform.y = reader.ReadFloat32();
        state.transform.rotation = reader.ReadFloat32();
        state.movement_key_held = reader.ReadBool();
        state.entity_id = reader.ReadUint32();
        const auto kind = reader.ReadUint8();
        if (kind >
            static_cast<std::uint8_t>(DoubleTimeEntityKind::Collectible)) {
            throw std::invalid_argument(
                "Unknown Double Time snapshot entity kind.");
        }
        state.kind = static_cast<DoubleTimeEntityKind>(kind);
        state.width = reader.ReadFloat32();
        state.height = reader.ReadFloat32();
        state.timeline_tics = reader.ReadUint64();
        state.enabled = reader.ReadBool();
        state.area_index = reader.ReadUint32();
        state.tilemap_index = reader.ReadUint32();
        if (!std::isfinite(state.transform.x) ||
            !std::isfinite(state.transform.y) ||
            !std::isfinite(state.transform.rotation) ||
            !std::isfinite(state.width) || !std::isfinite(state.height) ||
            state.width < 0.0F || state.height < 0.0F) {
            throw std::invalid_argument(
                "Invalid Double Time snapshot geometry.");
        }
        clocks += state.kind == DoubleTimeEntityKind::AudioTimeline ? 1 : 0;
        states.push_back(state);
    }
    if (clocks != 1) {
        throw std::invalid_argument(
            "Double Time snapshot requires one authoritative clock.");
    }
    std::optional<DoubleTimeRunStats> stats;
    if (reader.Remaining() == 2 * sizeof(std::uint32_t)) {
        stats = DoubleTimeRunStats{reader.ReadUint32(), reader.ReadUint32()};
    } else if (reader.Remaining() != 0) {
        throw std::invalid_argument("Invalid Double Time snapshot counters.");
    }
    return std::make_unique<DoubleTimeWorldSnapshotEvent>(std::move(states),
                                                          stats);
}
