#pragma once

#include "goose_session.hpp"
#include "goose_simulation.hpp"

#include <svanes/network/input_sync.hpp>
#include <svanes/network/peer_group.hpp>
#include <svanes/network/udp_msg_pipe.hpp>
#include <svanes/timeline_system.hpp>

#include <cstdint>
#include <memory>
#include <optional>

/** The session id every peer-to-peer copy of the game uses. */
inline constexpr svanes::SessionId GoosePeerSessionId = 1;

/** The port a new world listens on unless another is chosen. */
inline constexpr std::uint16_t GooseHostPort = 45000;

/** The port a joining process listens on unless another is chosen. */
inline constexpr std::uint16_t GooseJoinPort = 45001;

/**
 * A peer-to-peer world: every player runs the simulation and InputSync keeps
 * the copies identical by exchanging inputs. The world's clock feeds InputSync
 * its time, so pausing or changing the clock's speed pauses or changes the pace
 * of the shared simulation.
 */
class PeerSession final : public GooseSession {
public:
    /**
     * Starts a new world open for others to join, or joins an existing one.
     * @param simulation The simulation to keep in step with the other players.
     * @param port The local UDP port this process listens on.
     * @param join_address A player already in the world to join, or empty to start a new world.
     */
    PeerSession(GooseSimulation& simulation, std::uint16_t port, std::optional<svanes::UdpAddress> join_address);

    void Update(const svanes::FrameContext& frame, ClockRequest clock) override;
    bool IsRunning() const override;
    std::size_t PlayerCount() const override;
    svanes::PeerId LocalPeer() const override;
    void RequestLeave() override;
    bool CanClose() const override;
    std::string Status() const override;

private:
    std::unique_ptr<svanes::PeerGroup> network;
    std::unique_ptr<svanes::InputSync<GooseIntent, GooseIntentUse>> sync;
    svanes::Timeline world_clock;
    svanes::TicCount diagnostic_tics = 0;
};
