#pragma once

#include "combat/weapon_system.hpp"
#include "game_objects/dynamic_object/ship.hpp"
#include "game_objects/planet.hpp"
#include "propulsion_system.hpp"
#include "serialization/asset_catalog.hpp"
#include <map>
#include <span>
#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/game.hpp>
#include <svanes/network/input_sync.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/network/network_session.hpp>

// How many simulation tics advanced by one step.
inline constexpr svanes::TicCount OrbitalStepTics = 10000;

/**
 * One player's controls for a simulation tick.
 * 
 * FIELDS:
 * - propulsion: Thrust and rotation requests.
 * - weapons: A firing request.
 * - pause: A shared pause toggle.
 */
struct OrbitalInput {
    PropulsionControl propulsion{};
    WeaponControl weapons{};
    bool pause = false;
};

/**
 * Represents the game simulation for Orbital Escalation (used for multiplayer).
 * 
 * Owns the gameplay world and advances it one agreed input tick at a time.
 * Rendering may run more or less often without changing what a tick does.
 * 
 * The registry must outlive this object.
 */
class OrbitalSimulation final
    : public svanes::SyncedSimulation<OrbitalInput, std::uint8_t> {
public:
    /**
     * Save a deterministic snapshot of the simulation state in order to
     * use it for rolling back to a previous state, or to synchronize a new peer
     * joining the game, or for hash/correctness checks.
     */
    svanes::NetworkMessage Save() const override;

    /**
     * Load a deterministic snapshot of the simulation state in order to
     * restore the game to some state or synchronize a new peer.
     *
     * @param bytes The complete state produced by Save().
     */
    void Load(std::span<const std::byte> bytes) override;

    /**
     * Gets a hash of the simulation rules and definitions, which must match
     * between peers for gameplay to be compatible.
     *
     * @return The gameplay compatibility hash.
     */
    std::uint64_t RulesHash() const;

    /**
     * Creates and initializes the game simulation.
     *
     * @param world The registry that will contain gameplay and local visuals.
     * @param asset_root The folder containing the game's object definitions.
     * @param concurrency The number of physics workers, including the caller.
     */
    OrbitalSimulation(svanes::Registry &world,
                      const std::filesystem::path &asset_root,
                      std::uint32_t concurrency = 1);

    /**
     * Creates a ship and its attachments for a new player.
     * 
     * @param peer The unique, nonzero player ID. Add players in ascending
     * order.
     */
    void AddPlayer(svanes::PeerId peer) override;

    /**
     * Removes a player and their ship. Already launched projectiles remain.
     * 
     * @param peer The player leaving the roster.
     */
    void RemovePlayer(svanes::PeerId peer) override;

    /**
     * Gets the player's current ship, for camera tracking or other local
     * presentation.
     *
     * @param peer The player whose ship to look up.
     * @return The ship's local entity ID, or std::nullopt if the player is
     * absent or their ship has been destroyed.
     */
    std::optional<svanes::Entity> PlayerEntity(svanes::PeerId peer) const;

    /**
     * Records local controls for the next simulation tick. Call once per
     * rendered frame, including while waiting for other players' inputs.
     * Firing and pause presses remain available until TakeInput consumes them.
     * A player whose ship has been destroyed can still request a pause.
     *
     * @param frame The local input devices and camera.
     * @param peer The player controlled by these local input devices.
     */
    void CaptureInput(const svanes::FrameContext &frame,
                      svanes::PeerId peer) override;

    /**
     * Consumes the local controls for one simulation tick. Held controls
     * remain available for later ticks, while firing and pause presses are
     * each returned once.
     *
     * @return The latest held controls and any presses recorded since the
     * previous call.
     */
    OrbitalInput TakeInput() override;

    /**
     * Writes one player's controls to a network message, allowing another
     * peer to apply the same requests.
     *
     * @param writer The message to append the controls to.
     * @param input The controls for one simulation tick.
     *
     * @throws std::invalid_argument if any propulsion value is nonfinite or
     * outside [-1, 1].
     */
    void EncodeInput(svanes::MessageWriter &writer,
                     const OrbitalInput &input) const override;

    /**
     * Reads and validates one player's controls from a network message.
     *
     * @param reader The message positioned at controls written by EncodeInput.
     * @return The controls to apply for one simulation tick.
     *
     * @throws std::invalid_argument if the controls are truncated, a boolean
     * flag is invalid, or a propulsion value is nonfinite or outside [-1, 1].
     */
    OrbitalInput DecodeInput(svanes::MessageReader &reader) const override;

    /**
     * Advances gameplay by one fixed simulation step using the supplied
     * player controls. Each peer must call this with the same inputs.
     *
     * Pause requests toggle gameplay once per tick, even if several players
     * request it. While paused, ticks still accept pause requests, but firing
     * requests are discarded and gameplay time remains stopped.
     *
     * @param inputs One set of controls for every player, in ascending PeerId
     * order. Players whose ships have been destroyed still require an entry.
     *
     * @return One input-use report per player, in input order.
     *
     * @throws std::invalid_argument if the input count differs from the player
     * count, or a propulsion value is nonfinite or outside [-1, 1].
     * @throws std::logic_error if the world does not have exactly one
     * attractor.
     */
    std::vector<std::uint8_t>
    Step(std::span<const OrbitalInput> inputs) override;

    /**
     * Checks if gameplay is currently paused.
     *
     * @return true if gameplay is paused, false otherwise.
     */
    bool IsPaused() const;

    /**
     * Updates the visuals of the planet, ships, and projectiles for rendering.
     * Can be called between simulation steps without advancing gameplay.
     */
    void UpdateVisuals();

private:
    /**
     * Creates the planet definition so new games and restored snapshots use
     * the same planet properties.
     *
     * @return The planet's gravity, collision, and visual properties.
     */
    static PlanetDefinition MakePlanetDefinition();

    // The hash used by peers to check that their gameplay rules and
    // definitions are compatible.
    std::uint64_t rules_hash = 0;

    /**
     * Represents a player who is still part of the game, including after their
     * ship has been destroyed.
     *
     * FIELDS:
     * - ship: The player's living ship, or empty after its destruction.
     * - touching_planet: Whether the previous tick contacted the planet, so
     * staying in contact does not deal impact damage every tick.
     */
    struct Player {
        std::unique_ptr<DynamicObject> ship;
        bool touching_planet = false;
    };

    /**
     * Gives a gameplay entity an identity shared by all peers. Keeps any
     * identity it already has. New identities must be assigned in the same
     * object order on every peer.
     *
     * @param entity The local entity to assign a shared identity to.
     *
     * @throws std::overflow_error if no more shared identities are available.
     */
    void AssignIdentity(svanes::Entity entity);

    /**
     * Assigns shared identities to an object and all of its attachments.
     * Existing identities are preserved.
     *
     * @param object The object whose attachment tree to assign identities to.
     *
     * @throws std::overflow_error if no more shared identities are available.
     */
    void AssignIdentities(const DynamicObject &object);

    /**
     * Applies player controls and resolves combat after movement. Run once per
     * unpaused simulation step, after physics has advanced the objects.
     *
     * @param inputs The current tick's controls in ascending PeerId order.
     */
    void RespondToPhysics(std::span<const OrbitalInput> inputs);

    // The engine owned registry containing gameplay objects and their visuals.
    svanes::Registry &world;

    // The driver used to distribute physics and collision work across threads.
    svanes::AsyncParallelForDriver driver;

    // The parent timeline used to pause or change the speed of all gameplay.
    OwnedEntity gameplay_timeline;

    // The object definitions used to create ships and replacement ammunition.
    AssetCatalog assets;

    // The planet providing the world's gravity and surface collisions.
    std::optional<Planet> planet;

    // Players keyed by PeerId, so iteration follows an agreed input order.
    // A player remains here even after their ship is destroyed.
    std::map<std::uint32_t, Player> players;

    // Owns launched projectiles independently of the ships that fired them.
    std::vector<std::unique_ptr<DynamicObject>> projectiles;

    // Owns the temporary explosion visuals until their flashes have faded.
    std::vector<OwnedEntity> collision_flashes;

    // The next shared object identity. Identities are never reused, even after
    // the corresponding objects have been destroyed.
    std::uint64_t next_identity = 1;

    // The latest local propulsion request and any firing or pause presses
    // waiting for TakeInput to consume them.
    OrbitalInput pending_input;
};
