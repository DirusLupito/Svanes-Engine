#include "death_cam.hpp"

#include "world_snapshot_event.hpp"

#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/physics_system.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

void DeathCam::Initialize(svanes::NetworkServer &network) {

    // The broadcast function is used to send a world update to all clients.
    const auto broadcast = [this, &network](WorldUpdate snapshot) {
        snapshot.death_cam_player = player;
        snapshot.paused = false;
        network.Broadcast(snapshot.Write());
    };

    // Create a replay session that will handle the playback of recorded world
    // updates.
    auto &replay = events.CreateReplaySession(
        svanes::DefaultPhysicsStepTics,
        {[] { return svanes::NetworkMessage{}; },
         [broadcast](std::span<const std::byte> state) {
             broadcast(WorldUpdate::Read(svanes::NetworkMessage{
                 std::vector<std::byte>(state.begin(), state.end())}));
         },
         [](svanes::TicCount) {}});

    // Register the WorldSnapshotEvent with the replay session, providing
    // encoding and decoding functions.
    replay.RegisterEvent<WorldSnapshotEvent>(
        [](svanes::MessageWriter &writer, const WorldSnapshotEvent &event) {
            writer.WriteBytes(event.snapshot.Write().bytes);
        },
        [](svanes::MessageReader &reader) {
            const auto bytes = reader.ReadBytes(reader.Remaining());
            return std::make_unique<WorldSnapshotEvent>(
                WorldUpdate::Read(svanes::NetworkMessage{
                    std::vector<std::byte>(bytes.begin(), bytes.end())}));
        });

    // Subscribe to WorldSnapshotEvent and broadcast the snapshot to all clients
    // when the event is triggered.
    events.Subscribe(
        std::make_unique<svanes::EventHandlerWrapper<WorldSnapshotEvent>>(
            [broadcast](const WorldSnapshotEvent &event) {
                broadcast(event.snapshot);
            }));
}

void DeathCam::Capture(const WorldUpdate &snapshot) {
    // If a newer update at the same time exists, replace it with the latest
    // one.
    if (!history.empty() && history.back().tics == snapshot.total_tics) {
        history.pop_back();
    }

    // Save the new snapshot and discard any updates older than five gameplay
    // seconds.

    history.push_back(
        {snapshot.total_tics, snapshot.Write(), snapshot.players});

    while (history.size() > 1 && snapshot.total_tics - history.front().tics >
                                     svanes::SecondsToTics(5)) {
        history.pop_front();
    }
}

void DeathCam::Play(svanes::ClientId player) {
    if (IsPlaying()) {
        throw std::logic_error("A death cam is already playing.");
    }

    if (history.empty()) {
        throw std::logic_error("Death cam requires recorded world updates.");
    }

    auto first = history.size() - 1;
    std::optional<svanes::Entity> ship;

    // Stop at this ship's spawn so a short life cannot replay an earlier death.
    while (first > 0) {
        const auto &players = history[first - 1].players;
        const auto found = std::find_if(
            players.begin(), players.end(),
            [player](const auto &entry) { return entry.client == player; });

        if (found == players.end() || !found->ship ||
            (ship && found->ship != ship)) {
            break;
        }

        ship = found->ship;
        --first;
    }

    if (!ship) {
        throw std::logic_error(
            "Death cam has no starting state for the player.");
    }

    svanes::ReplayRecording recording;
    recording.step_tics = svanes::DefaultPhysicsStepTics;
    recording.initial_state = history[first].state;
    recording.steps.resize((history.back().tics - history[first].tics) /
                           recording.step_tics);

    // Add the recorded world updates to the replay steps, ensuring they are in
    // the correct order.
    for (auto index = first + 1; index < history.size(); ++index) {
        const auto step =
            (history[index].tics - history[first].tics) / recording.step_tics -
            1;

        recording.steps[step].push_back(
            {std::string(WorldSnapshotEvent::EventName), 0,
             history[index].state});
    }

    this->player = player;
    events.GetReplaySession().Play(std::move(recording));
}

void DeathCam::Update(svanes::TicCount elapsed_tics) {
    events.GetReplaySession().Update(elapsed_tics);
}

bool DeathCam::IsPlaying() { return events.GetReplaySession().IsReplaying(); }
