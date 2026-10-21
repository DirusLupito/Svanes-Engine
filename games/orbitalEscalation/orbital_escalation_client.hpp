#pragma once

#include "game_objects/planet.hpp"
#include "serialization/asset_catalog.hpp"
#include "serialization/world_update.hpp"

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/entity.hpp>
#include <svanes/network/clientserver/IClient.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * Implements Orbital Escalation's client view and local controls.
 */
class OrbitalEscalationClient final : public svanes::IClient {
public:
    /**
     * Configures the game to play in a world controlled by a server.
     *
     * @param server_address The server's address for admission and gameplay
     * messages.
     * @throws std::invalid_argument if the server address is empty or the
     * gameplay object definitions are invalid.
     */
    explicit OrbitalEscalationClient(std::string server_address);

    /**
     * Starts joining the server and prepares the client's view.
     * @param context The engine services used to initialize the client.
     */
    void Initialize(svanes::ClientContext &context) override;

    /**
     * Creates the game's join request for the server.
     * @return The game specific join request bytes.
     */
    svanes::NetworkMessage CreateJoinRequest() override;

    /**
     * Reports the outcome of joining the server.
     * @param result The admission decision or timeout reported by the engine.
     */
    void HandleJoinResult(const svanes::JoinResult &result) override;

    /**
     * Closes the game when its connection to the server is lost.
     */
    void HandleDisconnect() override;

    /**
     * Updates the client's view and local controls once per frame.
     * @param frame The context for the current frame, providing access to the
     * InputManager and the time elapsed since the last frame.
     */
    void Update(const svanes::ClientFrameContext &frame) override;

    /**
     * Checks whether the game is ready to close.
     *
     * @return true if the game should close, false otherwise.
     */
    bool ShouldQuit() const override;

    /**
     * Determines the interval between frames in tics.
     * This method is called once per frame, allowing the game to control the
     * pace of the main loop. The engine will sleep for the remaining time
     * until the next frame, if any.
     *
     * @return The interval between frames in tics. A tic is one microsecond.
     * Return 0 to disable pacing and run as fast as possible.
     */
    svanes::TicCount GetFrameIntervalTics() const override;

private:
    /**
     * Applies the server's complete world state to the client's
     * view of the game. This should be called once per frame if
     * the server has sent any world update messages.
     * Discards predicted-only gameplay objects and preserves local effects.
     *
     * TODO: Consider optimizing this to only update objects that have changed
     * since the last update, or perhaps only objects that the client can see or
     * interact with.
     *
     * @param frame The client's world, camera, and other services for this
     * frame.
     * @param update The world state received from the server.
     */
    void ApplyWorldUpdate(const svanes::ClientFrameContext &frame,
                          const WorldUpdate &update);

    /**
     * Reads this player's controls for local prediction and queues them for
     * the server to apply.
     *
     * @param frame The local input, displayed world, camera, and network client
     * for this frame.
     */
    void SendInput(const svanes::ClientFrameContext &frame);

    /**
     * Predicts movement, collisions, damage, and weapons between server
     * updates.
     *
     * @param world The client's world after its timelines have advanced.
     */
    void Predict(svanes::Registry &world);

    // The server's address for admission and gameplay messages.
    std::string server_address;

    // The definitions supplying the appearance and fixed properties of objects.
    AssetCatalog assets;

    // The server assigned ID of this player, or zero before admission.
    std::uint32_t client_id = 0;

    // The server entity ID of this player's ship, or empty before its first
    // world update or after its destruction.
    std::optional<svanes::Entity> player_ship;

    // The master clock shared by gameplay objects. Server updates replace its
    // state, and prediction advances it between those updates.
    std::optional<OwnedEntity> gameplay_timeline;

    // The local clock for visual effects. It follows the server's pause state,
    // but its elapsed time is never replaced by a server update.
    std::optional<OwnedEntity> effects_timeline;

    // The planet displayed in the client's world.
    std::optional<Planet> planet;

    // The server-backed gameplay objects, keyed by their server entity IDs.
    // Ownership belongs to root_objects or another object's attachment vector.
    std::unordered_map<svanes::Entity, DynamicObject *> objects;

    // The owners of independent gameplay objects. Each object owns its attached
    // objects, along with its local entity and visuals.
    std::vector<std::unique_ptr<DynamicObject>> root_objects;

    // List of collision flashes. Purely clientside.
    std::vector<OwnedEntity> collision_flashes;

    // The local IDs of ships touching the planet during the previous prediction
    // step, so sustained contact does not repeatedly deal impact damage.
    std::unordered_set<svanes::Entity> touching_planet;

    // The parfor driver used by local physics and collision detection.
    svanes::AsyncParallelForDriver parallel_for{1};

    // The timeline controlling the client's frame rate.
    svanes::Timeline client_loop_timeline;

    // The timeline used to animate the pause label.
    svanes::Timeline pause_timeline;

    // The text label displayed while gameplay is paused.
    svanes::Entity pause_label_entity = 0;

    // The background rectangle that follows the camera's view.
    svanes::Entity background_entity = 0;

    // Whether the game is ready to close.
    bool should_quit = false;
};
