#pragma once

#include "game_events.hpp"
#include "replay_protocol.hpp"

#include <svanes/events/event_manager.hpp>
#include <svanes/events/replay_session.hpp>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>

class DoubleTimeReplayServer final {
public:
    using Sender =
        std::function<void(svanes::ClientId, const svanes::NetworkMessage &)>;
    DoubleTimeReplayServer(std::filesystem::path path, Sender send,
                           svanes::NetworkMessage initial);
    void AddClient(svanes::ClientId client);
    void RemoveClient(svanes::ClientId client);
    void Request(svanes::ClientId client, DoubleTimeReplayMessage kind);
    void Tick(const DoubleTimeWorldSnapshotEvent &snapshot,
              svanes::TicCount elapsed);
    bool IsViewing(svanes::ClientId client) const;

private:
    void Status(svanes::ClientId client, const std::string &message);
    void StatusAll(const std::string &message);
    void SendSnapshot(svanes::ClientId client,
                      std::span<const std::byte> snapshot);
    void StopRecording();

    std::filesystem::path path;
    Sender send;
    svanes::NetworkMessage latest;
    svanes::EventManager events;
    svanes::ReplaySession &recording;
    std::set<svanes::ClientId> clients;
    std::map<svanes::ClientId, std::unique_ptr<svanes::EventManager>> viewers;
};
