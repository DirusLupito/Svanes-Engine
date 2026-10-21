#pragma once

#include "combat/weapon_system.hpp"
#include "game_objects/dynamic_object/ship.hpp"
#include "game_objects/planet.hpp"
#include "propulsion_system.hpp"
#include "serialization/asset_catalog.hpp"

#include <svanes/network/clientserver/IServer.hpp>

#include <map>
#include <optional>
#include <vector>

inline constexpr std::uint16_t OrbitalServerPort = 45010;

// The interval between sending server world updates.
inline constexpr svanes::TicCount OrbitalSnapshotTics = 50000; // 1/20 second

/**
 * Implements the server for Orbital Escalation. Responsible for managing the
 * authoritative game simulation and handling player connections.
 */
class OrbitalEscalationServer final : public svanes::IServer {
public:
    /**
     * Constructs an Orbital Escalation server with the specified asset folder
     * and joining port.
     *
     * @param assets The folder containing the gameplay object definitions.
     * @param joining_port The port accepting player join requests.
     * @throws std::invalid_argument if the asset folder or definitions are
     * invalid.
     */
    explicit OrbitalEscalationServer(
        const std::filesystem::path &assets,
        std::uint16_t joining_port = OrbitalServerPort);

    /**
     * Prepares Orbital Escalation's world and starts listening for players.
     *
     * @param context The engine services used to initialize the game server.
     *
     * @throws zmq::error_t if the listening pipe cannot be configured.
     */
    void Initialize(svanes::ServerContext &context) override;

    /**
     * Handles a client's request to join Orbital Escalation and creates their
     * player in the game world.
     *
     * @param context The client's join request and the server's game context.
     *
     * @return The admission decision and any game response for the client.
     *
     * @throws std::overflow_error if an available spawn position cannot be
     * represented.
     */
    svanes::JoinResponse
    HandleJoinRequest(const svanes::ServerJoinContext &context) override;

    /**
     * Removes a departing player and their ship, if it is still alive.
     *
     * @param world The server's game world.
     * @param client The ID of the player leaving the game.
     */
    void HandleDisconnect(svanes::Registry &world,
                          svanes::ClientId client) override;

    /**
     * Processes player controls and pause requests for the current
     * gameplay tick.
     *
     * @param context The game world and client inputs for this update.
     *
     * @throws std::invalid_argument if a client sends invalid controls.
     */
    void Update(const svanes::ServerUpdateContext &context) override;

    /**
     * Updates combat, weapons, and effects after the engine advances movement,
     * respecting the game's pause state.
     *
     * @param context The game world and engine services for post-physics work.
     */
    void PhysicsUpdate(const svanes::ServerPhysicsContext &context) override;

private:
    /**
     * Represents a client's controls for a single gameplay tick.
     *
     * FIELDS:
     * - propulsion: The latest held thrust and rotation requests.
     * - weapons: A firing request waiting for the next physics update.
     * - pause: A pause toggle waiting for the next update.
     */
    struct Input {
        PropulsionControl propulsion{};
        WeaponControl weapons{};
        bool pause = false;
    };

    /**
     * A player admitted by the server.
     *
     * FIELDS:
     * - ship: The player's ship, or empty after its destruction.
     * - input: The client's latest controls and pending presses.
     * - touching_planet: Whether the previous physics update contacted the
     * planet.
     */
    struct Player {
        std::unique_ptr<DynamicObject> ship;
        Input input;
        bool touching_planet = false;
    };

    /**
     * Decodes and validates a client's gameplay controls.
     *
     * @param message The game's control message received from a client.
     * @return The client's controls for the server to apply.
     * @throws std::invalid_argument if the message contains invalid controls.
     */
    static Input DecodeInput(const svanes::NetworkMessage &message);

    /**
     * Adds a player to the game and spawns their ship in an available position.
     *
     * @param world The engine owned registry in which to create the ship.
     * @param client The unique, nonzero client ID assigned by the server.
     * @throws std::invalid_argument if the ID is zero or already has a player.
     * @throws std::overflow_error if an available spawn position cannot be
     * represented.
     */
    void AddPlayer(svanes::Registry &world, svanes::ClientId client);

    /**
     * Applies player controls and resolves gameplay interactions after
     * movement, including collisions, damage, and destruction.
     *
     * @param world The authoritative world after the engine's physics step.
     * @param driver The engine's parallel for driver for collision detection.
     */
    void RespondToPhysics(svanes::Registry &world,
                          svanes::AsyncParallelForDriver &driver);

    /**
     * Creates a message containing the current game state for clients
     * to receive and use to update their local view of the world.
     *
     * @param world The server's current game world.
     *
     * @return The binary world state used to reconstruct the client's view.
     */
    svanes::NetworkMessage
    CreateWorldUpdate(const svanes::Registry &world) const;

    // The definitions used to create gameplay objects.
    AssetCatalog assets;

    // The port on which the server listens for clients.
    std::uint16_t port;

    // The master timeline for which all gameplay object timelines are children.
    // Pausing this timeline pauses the game.
    std::optional<OwnedEntity> gameplay_timeline;

    // The central  planet which attacts and collides with gameplay objects.
    std::optional<Planet> planet;

    // The stationary triangle and its repulsion field.
    std::optional<OwnedEntity> repelling_triangle;

    // The square that rotates around its fixed center.
    std::optional<OwnedEntity> rotating_square;

    // The players admitted to the game, indexed by client ID.
    std::map<svanes::ClientId, Player> players;

    // A list of all projectiles in the game, including missiles and their
    // attachments.
    std::vector<std::unique_ptr<DynamicObject>> projectiles;

    // Simulation time accumulated toward sending the next world update.
    svanes::TicCount snapshot_tics = 0;
};
