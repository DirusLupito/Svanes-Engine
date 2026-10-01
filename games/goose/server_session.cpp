#include "server_session.hpp"

#include <svanes/network/udp_msg_pipe.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

constexpr std::chrono::milliseconds kSlotAnswerTime{500};
constexpr svanes::TicCount kMaxInputBacklog = 4 * GooseStepTics;
constexpr std::uint32_t kGoodbyeRepeats = 3;

/**
 * @param roster A roster.
 * @param peer A player.
 * @return Whether the player is in the roster.
 */
bool Contains(const std::vector<svanes::PeerId>& roster, svanes::PeerId peer)
{
    return std::find(roster.begin(), roster.end(), peer) != roster.end();
}

}

ServerSession::ServerSession(GooseSimulation& simulation, std::string host)
    : simulation(simulation), host(std::move(host)), id(svanes::GenerateClientId())
{
    ConnectToSlot(0);
}

void ServerSession::ConnectToSlot(std::uint32_t next_slot)
{
    if (next_slot >= GooseServerSlots) {
        throw std::runtime_error("Every slot on the goose server at " + host + " is taken or not answering.");
    }
    slot = next_slot;
    client = std::make_unique<svanes::NetworkClient>(std::make_unique<svanes::UdpMsgPipe>(
        static_cast<std::uint16_t>(0), host, static_cast<std::uint16_t>(GooseServerFirstPort + slot)
    ));
    slot_deadline = std::chrono::steady_clock::now() + kSlotAnswerTime;
    client->Send(EncodeHello(id));
}

void ServerSession::Update(const svanes::FrameContext& frame, ClockRequest clock)
{
    if (left) {
        return;
    }
    const std::uint32_t polled_slot = slot;
    for (const svanes::NetworkMessage& message : client->PollBroadcast()) {
        if (slot != polled_slot) {
            break;
        }
        Handle(DecodeGooseMessage(message, simulation));
    }
    const auto now = std::chrono::steady_clock::now();
    if (!local) {
        if (now >= slot_deadline) {
            ConnectToSlot(slot + 1);
        } else {
            client->Send(EncodeHello(id));
        }
        return;
    }
    if (now - last_heard > GooseServerTimeout) {
        throw std::runtime_error("Lost the goose server at " + host + ".");
    }
    if (!IsRunning()) {
        return;
    }

    simulation.CaptureInput(frame, *local);
    input_tics += std::min(frame.real_delta_tics, kMaxInputBacklog - input_tics);
    while (input_tics >= GooseStepTics) {
        const float unsent_dash = outgoing.dash;
        outgoing = simulation.TakeInput();
        if (outgoing.dash == 0.0F) {
            outgoing.dash = unsent_dash;
        }
        input_tics -= GooseStepTics;
    }
    client->Send(EncodeInput(simulation, outgoing, clock));
    outgoing.dash = 0.0F;
}

void ServerSession::Handle(const GooseMessage& message)
{
    switch (message.type) {
    case GooseMessageType::Welcome:
        if (!local && message.client == id) {
            local = message.peer;
            last_heard = std::chrono::steady_clock::now();
            std::cout << "Joined the goose server at " << host << " in slot " << slot
                << " as player " << message.peer.value << ".\n";
        }
        return;
    case GooseMessageType::Taken:
        if (!local && message.client == id) {
            ConnectToSlot(slot + 1);
        }
        return;
    case GooseMessageType::Snapshot:
        if (local) {
            last_heard = std::chrono::steady_clock::now();
            ApplySnapshot(message);
        }
        return;
    case GooseMessageType::Hello:
    case GooseMessageType::Input:
    case GooseMessageType::Goodbye:
        break;
    }
    throw std::runtime_error("The goose server sent a message only clients send.");
}

void ServerSession::ApplySnapshot(const GooseMessage& snapshot)
{
    if (!Contains(snapshot.roster, *local)) {
        if (IsRunning()) {
            throw std::runtime_error("The goose server removed this player.");
        }
        return;
    }
    for (const svanes::PeerId peer : roster) {
        if (!Contains(snapshot.roster, peer)) {
            simulation.RemovePlayer(peer);
        }
    }
    for (const svanes::PeerId peer : snapshot.roster) {
        if (!Contains(roster, peer)) {
            simulation.AddPlayer(peer);
        }
    }
    roster = snapshot.roster;
    simulation.Load(snapshot.world);
}

bool ServerSession::IsRunning() const
{
    return !left && local && Contains(roster, *local);
}

std::size_t ServerSession::PlayerCount() const
{
    if (!IsRunning()) {
        return 0;
    }
    return roster.size();
}

svanes::PeerId ServerSession::LocalPeer() const
{
    return local.value_or(svanes::PeerId{});
}

void ServerSession::RequestLeave()
{
    if (local) {
        for (std::uint32_t repeat = 0; repeat < kGoodbyeRepeats; ++repeat) {
            client->Send(EncodeGoodbye());
        }
    }
    left = true;
}

bool ServerSession::CanClose() const
{
    return left;
}

std::string ServerSession::Status() const
{
    if (left) {
        return "Left the goose server.";
    }
    if (!local) {
        return "Looking for a free slot on the goose server at " + host + ", trying slot " + std::to_string(slot) + ".";
    }
    if (!IsRunning()) {
        return "Waiting for the first snapshot from the goose server.";
    }
    return "Player " + std::to_string(local->value) + " on the goose server with "
        + std::to_string(roster.size()) + " player(s).";
}
