#pragma once

#include <svanes/game.hpp>
#include <svanes/network/network_session.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/utility/rational_number.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * The speeds the pause menu offers, slowest first.
 * - Half: Half of real time.
 * - Normal: Real time.
 * - Double: Twice real time.
 */
enum class GooseSpeed : std::uint8_t {
    Half,
    Normal,
    Double,
};

/**
 * @param speed A speed from the pause menu.
 * @return The tic size that runs a timeline at that speed, in parent tics per local tic.
 */
svanes::RationalNumber TicSize(GooseSpeed speed);

/**
 * How the local player wants the world's clock to run.
 * FIELDS:
 * - speed: The speed applied in the pause menu.
 * - paused: Whether the pause menu is open.
 */
struct ClockRequest {
    GooseSpeed speed = GooseSpeed::Normal;
    bool paused = false;
};

/**
 * Advances a world clock by real time. The request is followed only while one
 * player is in the world; otherwise the clock runs at 1x, so no player ever
 * sees the world pause or change pace because of someone else.
 * @param clock The world clock.
 * @param real_tics Real time that passed.
 * @param request What the only player wants, if they are alone.
 * @param alone Whether exactly one player is in the world.
 * @return The world time that passed, in tics.
 */
svanes::TicCount AdvanceWorldClock(
    svanes::Timeline& clock, svanes::TicCount real_tics, ClockRequest request, bool alone
);

/**
 * A goose world this process takes part in, either peer-to-peer or through a
 * goose server. The game drives it once per frame and reads back who is playing.
 */
class GooseSession {
public:
    virtual ~GooseSession() = default;

    /**
     * Exchanges messages and advances the shared simulation.
     * @param frame The current frame.
     * @param clock What the local player wants the world's clock to do.
     */
    virtual void Update(const svanes::FrameContext& frame, ClockRequest clock) = 0;

    /** @return Whether the world is loaded and the local player is in it. */
    virtual bool IsRunning() const = 0;

    /** @return How many players are in the world, or zero before it is running. */
    virtual std::size_t PlayerCount() const = 0;

    /** @return The local player's id, valid while running. */
    virtual svanes::PeerId LocalPeer() const = 0;

    /** Starts leaving the world. */
    virtual void RequestLeave() = 0;

    /** @return Whether leaving has finished and the process can close. */
    virtual bool CanClose() const = 0;

    /** @return A one-line description of the connection for the console. */
    virtual std::string Status() const = 0;
};
