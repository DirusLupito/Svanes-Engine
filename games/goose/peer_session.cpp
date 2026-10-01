#include "peer_session.hpp"

#include <svanes/utility/hash.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>

namespace {

/**
 * @param sync_settings The input sync pacing and limits.
 * @return A hash of everything two builds must share to play together.
 */
std::uint64_t RulesHash(const svanes::SyncSettings& sync_settings)
{
    svanes::MessageWriter writer;
    writer.WriteUint64(sync_settings.step_tics);
    writer.WriteFloat32(GooseGravity);
    writer.WriteUint64(sync_settings.prediction_ticks);
    writer.WriteUint64(sync_settings.history_ticks);
    writer.WriteUint64(sync_settings.hash_interval_ticks);
    writer.WriteUint64(sync_settings.max_unchecked_ticks);
    return svanes::HashBytes(writer.Finish().bytes);
}

}

PeerSession::PeerSession(
    GooseSimulation& simulation, std::uint16_t port, std::optional<svanes::UdpAddress> join_address
)
{
    const svanes::SyncSettings sync_settings{.step_tics = GooseStepTics};
    const svanes::PeerSettings settings{GoosePeerSessionId, GooseMaxPlayers, RulesHash(sync_settings)};
    auto pipe = std::make_unique<svanes::UdpMsgPipe>(port);
    if (join_address) {
        network = std::make_unique<svanes::PeerGroup>(std::move(pipe), *join_address, settings);
    } else {
        network = std::make_unique<svanes::PeerGroup>(std::move(pipe), settings);
    }
    sync = std::make_unique<svanes::InputSync<GooseIntent, GooseIntentUse>>(*network, simulation, sync_settings);
    if (!join_address) {
        std::cout << "Started a new world. Others can join on port " << network->Port() << ".\n";
    }
}

void PeerSession::Update(const svanes::FrameContext& frame, ClockRequest clock)
{
    const svanes::FrameContext world_frame{
        .world = frame.world,
        .input = frame.input,
        .real_delta_tics = AdvanceWorldClock(world_clock, frame.real_delta_tics, clock, PlayerCount() == 1),
        .output_width = frame.output_width,
        .output_height = frame.output_height,
        .audio = frame.audio,
        .camera = frame.camera,
        .gravity = frame.gravity,
    };

    const bool was_running = network->IsRunning();
    sync->Update(world_frame);
    if (!was_running && network->IsRunning()) {
        std::cout << "Joined at tick " << sync->Tick() << " with "
            << network->Peers().size() << " player(s). Others can join on port " << network->Port() << ".\n";
    }
    if (!network->IsRunning()) {
        return;
    }
    diagnostic_tics += std::min(frame.real_delta_tics, svanes::TicsPerSecond - diagnostic_tics);
    if (diagnostic_tics >= svanes::TicsPerSecond) {
        std::cout << "Peer " << network->LocalPeer().value << ": " << sync->Diagnostics() << '\n';
        diagnostic_tics = 0;
    }
}

bool PeerSession::IsRunning() const
{
    return network->IsRunning();
}

std::size_t PeerSession::PlayerCount() const
{
    if (!network->IsRunning()) {
        return 0;
    }
    return network->Peers().size();
}

svanes::PeerId PeerSession::LocalPeer() const
{
    return network->LocalPeer();
}

void PeerSession::RequestLeave()
{
    sync->RequestLeave();
}

bool PeerSession::CanClose() const
{
    return sync->CanClose();
}

std::string PeerSession::Status() const
{
    if (!network->IsRunning()) {
        return sync->Status();
    }
    return "Peer " + std::to_string(network->LocalPeer().value) + ": " + sync->Status();
}
