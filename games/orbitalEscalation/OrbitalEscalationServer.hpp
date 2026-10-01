#pragma once

#include "orbital_simulation.hpp"
#include <svanes/network/network_server.hpp>
#include <svanes/registry.hpp>

// Each listening port reserves one player slot on the server.
inline constexpr std::uint16_t OrbitalServerFirstPort = 45010;
inline constexpr std::uint16_t OrbitalServerSlots = 3;
// The interval for sharing server snapshots and sending client controls.
inline constexpr svanes::TicCount OrbitalSnapshotTics = 50000; // 1/20 second

/**
 * Owns the authoritative world, applies client controls, and shares snapshots
 * so connected players see the same game.
 */
class OrbitalEscalationServer {
public:
    /**
     * Creates the world and prepares the connections for its player slots.
     *
     * @param assets The folder containing the gameplay object definitions.
     * @throws std::invalid_argument if the asset folder or definitions are
     * invalid.
     */
    explicit OrbitalEscalationServer(const std::filesystem::path &assets);

    /**
     * Runs the server continuously, receiving controls and advancing gameplay.
     * Errors from input handling and simulation propagate to the caller.
     *
     * @throws std::invalid_argument if a client sends invalid controls.
     * @throws std::overflow_error if admitting a player exhausts spawn
     * positions or shared identities.
     * @throws std::logic_error if the world does not have exactly one
     * attractor.
     */
    void Run();

private:
    /**
     * Accepts a client's controls and admits its player on the first message.
     *
     * @param message The controls and the connection they arrived on.
     * @throws std::invalid_argument if the controls are truncated or invalid.
     * @throws std::overflow_error if the new player's spawn position or shared
     * identities cannot be represented.
     */
    void Receive(const svanes::ServerMessage &message);

    /**
     * Advances the authoritative world by one tick and shares snapshots when
     * due, so clients can display the resulting gameplay.
     *
     * @throws std::logic_error if the world does not have exactly one
     * attractor.
     */
    void Step();

    // Owns the entities in the authoritative world.
    svanes::Registry world;

    // Applies the gameplay rules to the server's world.
    OrbitalSimulation simulation;

    // Receives controls and sends snapshots through the player connections.
    svanes::NetworkServer network;

    // Latest held controls and pending presses, ordered by player ID.
    std::map<std::uint32_t, OrbitalInput> inputs;

    // Simulation time accumulated toward the next snapshot broadcast.
    svanes::TicCount snapshot_tics = 0;
};
