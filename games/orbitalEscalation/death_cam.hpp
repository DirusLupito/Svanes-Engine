#pragma once

#include "serialization/world_update.hpp"

#include <svanes/events/event_manager.hpp>
#include <svanes/network/clientserver/network_server.hpp>

#include <deque>

/**
 * Represents a short replay of a player's death, which can be broadcast to all
 * clients without affecting the authoritative game state.
 */
class DeathCam final {
public:
    /**
     * Prepares snapshot playback over the server's existing connections.
     *
     * @param network The server used to broadcast recorded world updates.
     * Must outlive this death cam.
     *
     * @throws std::logic_error if playback has already been initialized.
     */
    void Initialize(svanes::NetworkServer &network);

    /**
     * Saves a world update and discards updates older than five gameplay
     * seconds. A newer update at the same time replaces the previous one.
     *
     * @param snapshot The complete authoritative state at a simulation
     * boundary.
     */
    void Capture(const WorldUpdate &snapshot);

    /**
     * Starts broadcasting the selected player's final five seconds, beginning
     * later if their current ship spawned within that interval.
     *
     * @param player The last player found dead during the current physics
     * step. The latest captured update must contain their death.
     *
     * @throws std::logic_error if no earlier update contains the player's ship,
     * or playback is already active.
     *
     * @throws ... Propagates snapshot decoding or network errors.
     */
    void Play(svanes::ClientId player);

    /**
     * Advances the shared replay while the authoritative game is frozen.
     *
     * @param elapsed_tics The unscaled time elapsed since the last replay
     * update.
     *
     * @throws ... Propagates replay or network errors.
     */
    void Update(svanes::TicCount elapsed_tics);

    /**
     * Checks whether clients are being shown a recorded death.
     *
     * @return true until the last recorded step has played, false otherwise.
     */
    bool IsPlaying();

private:
    /**
     * An authoritative world update saved for a possible death cam.
     *
     * FIELDS:
     * - tics: The gameplay time at which this state was captured.
     * - state: The serialized world state used as a starting state or event.
     * - players: The roster used to find the start of a player's current life.
     */
    struct Snapshot {
        svanes::TicCount tics;
        svanes::NetworkMessage state;
        std::vector<WorldUpdate::Player> players;
    };

    // The captured world updates, ordered from oldest to newest.
    std::deque<Snapshot> history;

    // The event manager and replay session used to broadcast historical states.
    svanes::EventManager events;

    // The player whose ship clients should follow during playback.
    svanes::ClientId player = 0;
};
