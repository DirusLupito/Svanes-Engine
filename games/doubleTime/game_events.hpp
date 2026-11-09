#pragma once

#include "network_protocol.hpp"

#include <svanes/events/event.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

class DoubleTimeCommandEvent final
    : public svanes::EventOf<DoubleTimeCommandEvent> {
public:
    explicit DoubleTimeCommandEvent(PlayerInputMessage input) : input(input) {}
    static constexpr std::string_view EventName = "double_time.command";
    PlayerInputMessage input;
};

class DoubleTimePlayerSpawnEvent final
    : public svanes::EventOf<DoubleTimePlayerSpawnEvent> {
public:
    explicit DoubleTimePlayerSpawnEvent(svanes::ClientId client)
        : client(client) {}
    static constexpr std::string_view EventName = "double_time.player.spawn";
    svanes::ClientId client;
};

class DoubleTimePlayerDisconnectedEvent final
    : public svanes::EventOf<DoubleTimePlayerDisconnectedEvent> {
public:
    explicit DoubleTimePlayerDisconnectedEvent(svanes::ClientId client)
        : client(client) {}
    static constexpr std::string_view EventName =
        "double_time.player.disconnect";
    svanes::ClientId client;
};

class DoubleTimePlayerDeathEvent final
    : public svanes::EventOf<DoubleTimePlayerDeathEvent> {
public:
    explicit DoubleTimePlayerDeathEvent(svanes::Entity entity)
        : entity(entity) {}
    static constexpr std::string_view EventName = "double_time.player.death";
    svanes::Entity entity;
};

class DoubleTimeRespawnEvent final
    : public svanes::EventOf<DoubleTimeRespawnEvent> {
public:
    static constexpr std::string_view EventName =
        "double_time.character.respawn";
};

class DoubleTimeCollectEvent final
    : public svanes::EventOf<DoubleTimeCollectEvent> {
public:
    explicit DoubleTimeCollectEvent(svanes::Entity entity) : entity(entity) {}
    static constexpr std::string_view EventName = "double_time.collect";
    svanes::Entity entity;
};

class DoubleTimeAdvanceLevelEvent final
    : public svanes::EventOf<DoubleTimeAdvanceLevelEvent> {
public:
    static constexpr std::string_view EventName = "double_time.level.advance";
};

class OnBeatEvent final : public svanes::EventOf<OnBeatEvent> {
public:
    explicit OnBeatEvent(std::uint64_t beat_index) : beat_index(beat_index) {}
    static constexpr std::string_view EventName = "double_time.rhythm.beat";
    std::uint64_t beat_index;
};

struct DoubleTimeRunStats {
    std::uint32_t deaths = 0;
    std::uint32_t coins = 0;
};

class DoubleTimeWorldSnapshotEvent final
    : public svanes::EventOf<DoubleTimeWorldSnapshotEvent> {
public:
    explicit DoubleTimeWorldSnapshotEvent(
        std::vector<DoubleTimeEntityState> objects,
        std::optional<DoubleTimeRunStats> stats)
        : objects(std::move(objects)), stats(stats) {}
    static constexpr std::string_view EventName = "double_time.world.snapshot";
    svanes::NetworkMessage Write() const;
    static std::unique_ptr<DoubleTimeWorldSnapshotEvent>
    Read(const svanes::NetworkMessage &message);
    std::vector<DoubleTimeEntityState> objects;
    std::optional<DoubleTimeRunStats> stats;
};
