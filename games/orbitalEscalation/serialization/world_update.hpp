#pragma once

#include "../attachment_system.hpp"
#include "../game_objects/dynamic_object/dynamic_object.hpp"

#include <svanes/network/message_serialization.hpp>
#include <svanes/timeline_system.hpp>

#include <optional>
#include <vector>

/**
 * The server's complete description of the world displayed by clients.
 * Object definitions supply appearance and fixed properties. Server entity IDs
 * identify objects and references between them. Objects absent from a newer
 * update have left the world.
 */
class WorldUpdate {
public:
    /**
     * A player's association with a ship.
     *
     * FIELDS:
     * - client: The player's client ID.
     * - ship: The ship's server entity ID, or empty after its destruction.
     */
    struct Player {
        std::uint32_t client;
        std::optional<svanes::Entity> ship;
    };

    /**
     * An object's current movement.
     *
     * FIELDS:
     * - linear: The object's linear velocity, in world units per local tic.
     * - angular: The object's angular velocity, in radians per local tic.
     */
    struct Velocity {
        svanes::Vector2D linear;
        float angular;
    };

    /**
     * An object to create or update in the client's world.
     *
     * FIELDS:
     * - entity: The server's entity ID.
     * - type: The kind of object definition.
     * - definition: The asset catalog definition used to create the object.
     * - transform: The object's world pose.
     * - local_tics: The object's elapsed local time.
     * - velocity: The object's movement, if it moves independently.
     * - health: The remaining health, if the object can receive damage.
     * - attachment: The owner's server entity ID and relative pose, if
     * attached.
     * - color: An optional override for the object's solid visual colors.
     */
    struct Object {
        svanes::Entity entity;
        DynamicObjectType type;
        std::string definition;
        svanes::Transform transform;
        svanes::TicCount local_tics;
        std::optional<Velocity> velocity;
        std::optional<float> health;
        std::optional<Attachment> attachment;
        std::optional<svanes::Color> color;
    };

    /**
     * Encodes the world state for transmission to clients.
     * @return The binary world update.
     */
    svanes::NetworkMessage Write() const;

    /**
     * Decodes a world update received from the server.
     * @param message The game's world update message.
     * @return The server's world state.
     * @throws std::invalid_argument if the message is truncated or invalid.
     */
    static WorldUpdate Read(const svanes::NetworkMessage &message);

    // The elapsed time on the master gameplay clock.
    svanes::TicCount total_tics = 0;

    // The number of source tics per gameplay tic, controlling the game's speed.
    svanes::RationalNumber tic_size{1};

    // Whether the master gameplay clock is paused.
    bool paused = false;

    // The server entity ID of the planet, whose physical properties and
    // appearance are defined by shared game code.
    svanes::Entity planet;

    // The planet's position and rotation in the server's world.
    svanes::Transform planet_transform;

    // The square's authoritative rotation around its fixed center.
    float square_rotation = 0.0F;

    // All admitted players, including those whose ships have been destroyed.
    std::vector<Player> players;

    // All living gameplay objects, including attached objects and loaded ammo.
    std::vector<Object> objects;
};
