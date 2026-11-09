#include "replay_server.hpp"

#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/replay_events.hpp>
#include <svanes/physics_system.hpp>

namespace {
void RegisterSnapshots(svanes::ReplaySession &session) {
    session.RegisterEvent<DoubleTimeWorldSnapshotEvent>(
        [](svanes::MessageWriter &writer,
           const DoubleTimeWorldSnapshotEvent &event) {
            writer.WriteBytes(event.Write().bytes);
        },
        [](svanes::MessageReader &reader) {
            const auto bytes = reader.ReadBytes(reader.Remaining());
            return DoubleTimeWorldSnapshotEvent::Read(svanes::NetworkMessage{
                std::vector<std::byte>(bytes.begin(), bytes.end())});
        });
}
}

DoubleTimeReplayServer::DoubleTimeReplayServer(std::filesystem::path path,
                                               Sender send,
                                               svanes::NetworkMessage initial)
    : path(std::move(path)), send(std::move(send)), latest(std::move(initial)),
      recording(events.CreateReplaySession(svanes::DefaultPhysicsStepTics,
                                           {[this] { return latest; },
                                            [](std::span<const std::byte>) {},
                                            [](svanes::TicCount) {}})) {
    RegisterSnapshots(recording);
}

bool DoubleTimeReplayServer::IsViewing(svanes::ClientId client) const {
    return viewers.contains(client);
}

void DoubleTimeReplayServer::AddClient(svanes::ClientId client) {
    if (clients.insert(client).second) {
        Status(client, "Replay controls: [ record, ] save, \\ play/exit.");
    }
}

void DoubleTimeReplayServer::RemoveClient(svanes::ClientId client) {
    viewers.erase(client);
    clients.erase(client);
}

void DoubleTimeReplayServer::Status(svanes::ClientId client,
                                    const std::string &message) {
    auto writer = DoubleTimeReplayWriter(DoubleTimeReplayMessage::Status);
    writer.WriteBool(recording.GetState() ==
                     svanes::ReplaySession::State::Recording);
    writer.WriteBool(IsViewing(client));
    writer.WriteText(message);
    send(client, writer.Finish());
}

void DoubleTimeReplayServer::StatusAll(const std::string &message) {
    for (const auto client : clients) {
        Status(client, message);
    }
}

void DoubleTimeReplayServer::SendSnapshot(svanes::ClientId client,
                                          std::span<const std::byte> snapshot) {
    auto writer = DoubleTimeReplayWriter(DoubleTimeReplayMessage::Snapshot);
    writer.WriteBytes(snapshot);
    send(client, writer.Finish());
}

void DoubleTimeReplayServer::StopRecording() {
    if (recording.GetState() == svanes::ReplaySession::State::Recording) {
        events.TriggerEvent(svanes::StopRecordingEvent{});
        recording.ProcessControls();
        StatusAll("Recording saved: " + path.string());
    }
}

void DoubleTimeReplayServer::Request(svanes::ClientId client,
                                     DoubleTimeReplayMessage kind) {
    AddClient(client);
    try {
        switch (kind) {
        case DoubleTimeReplayMessage::Record:
            if (IsViewing(client)) {
                Status(client, "Exit playback before starting a recording.");
                return;
            }
            StopRecording();
            events.TriggerEvent(svanes::StartRecordingEvent{path});
            recording.ProcessControls();
            StatusAll(
                "Recording started; saving will replace the previous replay.");
            break;
        case DoubleTimeReplayMessage::Save:
            StopRecording();
            break;
        case DoubleTimeReplayMessage::Play: {
            if (IsViewing(client)) {
                return;
            }
            StopRecording();
            auto viewer = std::make_unique<svanes::EventManager>();
            viewer->Subscribe(
                std::make_unique<
                    svanes::EventHandlerWrapper<DoubleTimeWorldSnapshotEvent>>(
                    [this, client](const DoubleTimeWorldSnapshotEvent &event) {
                        SendSnapshot(client, event.Write().bytes);
                    }));
            auto &session = viewer->CreateReplaySession(
                svanes::DefaultPhysicsStepTics,
                {[this] { return latest; },
                 [this, client](std::span<const std::byte> bytes) {
                     DoubleTimeWorldSnapshotEvent::Read(svanes::NetworkMessage{
                         std::vector<std::byte>(bytes.begin(), bytes.end())});
                     SendSnapshot(client, bytes);
                 },
                 [](svanes::TicCount) {}});
            RegisterSnapshots(session);
            viewer->TriggerEvent(svanes::PlayReplayEvent{path});
            session.ProcessControls();
            if (session.IsReplaying()) {
                viewers.emplace(client, std::move(viewer));
                Status(client, "Replay playing. Press \\ or Escape to return "
                               "to live play.");
            } else {
                Status(client, "Replay finished; returned to live play.");
            }
            break;
        }
        case DoubleTimeReplayMessage::Exit:
            if (const auto found = viewers.find(client);
                found != viewers.end()) {
                found->second->TriggerEvent(svanes::StopReplayEvent{});
                found->second->GetReplaySession().ProcessControls();
                viewers.erase(found);
            }
            Status(client, "Returned to live play.");
            break;
        default:
            throw std::invalid_argument("Invalid replay control request.");
        }
    } catch (const std::exception &error) {
        Status(client, std::string{"Replay error: "} + error.what());
    }
}

void DoubleTimeReplayServer::Tick(const DoubleTimeWorldSnapshotEvent &snapshot,
                                  svanes::TicCount elapsed) {
    latest = snapshot.Write();
    if (recording.GetState() == svanes::ReplaySession::State::Recording) {
        recording.BeginStep();
        events.TriggerEvent(snapshot);
        recording.EndStep();
    }
    for (auto it = viewers.begin(); it != viewers.end();) {
        auto &session = it->second->GetReplaySession();
        session.Update(elapsed);
        if (!session.IsReplaying()) {
            const auto client = it->first;
            it = viewers.erase(it);
            Status(client, "Replay finished; returned to live play.");
        } else {
            ++it;
        }
    }
}
