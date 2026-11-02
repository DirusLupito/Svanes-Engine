#pragma once

#include "network_protocol.hpp"

#include <svanes/events/event.hpp>

#include <memory>
#include <utility>
#include <vector>

class DoubleTimeCommandEvent final : public svanes::Event {
public:
    explicit DoubleTimeCommandEvent(PlayerInputMessage input) : input(input) {}
    static std::string GetStaticEventType() { return "double_time.command"; }
    const std::string GetEventType() const override {
        return GetStaticEventType();
    }
    PlayerInputMessage input;
};

class DoubleTimePlayerSpawnEvent final : public svanes::Event {
public:
    explicit DoubleTimePlayerSpawnEvent(svanes::ClientId client)
        : client(client) {}
    static std::string GetStaticEventType() {
        return "double_time.player.spawn";
    }
    const std::string GetEventType() const override {
        return GetStaticEventType();
    }
    svanes::ClientId client;
};

class DoubleTimePlayerDisconnectedEvent final : public svanes::Event {
public:
    explicit DoubleTimePlayerDisconnectedEvent(svanes::ClientId client)
        : client(client) {}
    static std::string GetStaticEventType() {
        return "double_time.player.disconnect";
    }
    const std::string GetEventType() const override {
        return GetStaticEventType();
    }
    svanes::ClientId client;
};

class DoubleTimeRespawnEvent final : public svanes::Event {
public:
    static std::string GetStaticEventType() {
        return "double_time.character.respawn";
    }
    const std::string GetEventType() const override {
        return GetStaticEventType();
    }
};

class DoubleTimeCollectEvent final : public svanes::Event {
public:
    explicit DoubleTimeCollectEvent(svanes::Entity entity) : entity(entity) {}
    static std::string GetStaticEventType() { return "double_time.collect"; }
    const std::string GetEventType() const override {
        return GetStaticEventType();
    }
    svanes::Entity entity;
};

class DoubleTimeAdvanceLevelEvent final : public svanes::Event {
public:
    static std::string GetStaticEventType() {
        return "double_time.level.advance";
    }
    const std::string GetEventType() const override {
        return GetStaticEventType();
    }
};

class DoubleTimeWorldSnapshotEvent final : public svanes::Event {
public:
    explicit DoubleTimeWorldSnapshotEvent(
        std::vector<DoubleTimeEntityState> objects)
        : objects(std::move(objects)) {}
    static std::string GetStaticEventType() {
        return "double_time.world.snapshot";
    }
    const std::string GetEventType() const override {
        return GetStaticEventType();
    }
    svanes::NetworkMessage Write() const;
    static std::unique_ptr<DoubleTimeWorldSnapshotEvent>
    Read(const svanes::NetworkMessage &message);
    std::vector<DoubleTimeEntityState> objects;
};
