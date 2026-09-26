#pragma once

#include <svanes/entity.hpp>
#include <svanes/game.hpp>

#include <vector>

/**
 * Top level container for the Orbital Escalation game.
 * Used to hold bridge components responsible for talking
 * to the engine.
 */
class OrbitalEscalationGame final : public svanes::IGame {
public:
    /**
     * Initializes the game with the provided context.
     * @param context The context for the game, providing access to the
     * TextureManager.
     */
    void Initialize(svanes::GameContext &context) override;

    /**
     * Game specific update logic. Called by the engine once per frame.
     * @param frame The context for the current frame, providing access to the
     * InputManager and the time elapsed since the last frame.
     */
    void Update(const svanes::FrameContext &frame) override;

    /**
     * Post-physics update logic. Called by the engine once per simulation step
     * immediately after the physics system has updated all entities' for this
     * one single simulation step. This runs at most once per rendered frame,
     * before animation and Update, and is skipped when no step is due.
     *
     * @param physics The world, sampled input, and entities' local tic deltas.
     */
    void PhysicsUpdate(const svanes::PhysicsContext &physics) override;

    bool ShouldQuit() const override;

private:
    // The overarching timeline entity for which all other timelines are
    // children.
    svanes::Entity gameplay_timeline_entity = 0;

    svanes::Entity pause_timeline_entity = 0;
    svanes::Entity pause_label_entity = 0;


    svanes::Entity background_entity = 0;
    svanes::Entity square_entity = 0;
    svanes::Entity planet_entity = 0;
    std::vector<svanes::Entity> boundary_entities;

    // How many NPC entities to spawn in orbit around the planet
    uint32_t num_npc_entities_to_spawn = 1000;

    // The maximum magnitude of the initial velocity of the NPC entities
    // as given by the magnitude of the tangent to the vector from the planet to
    // the NPC entity when it is first spawned.
    float maximum_magnitude_of_npc_entity_initial_velocity = 4200.0F;

    // The minimum magnitude of the initial velocity of the NPC entities
    // as given by the magnitude of the tangent to the vector from the planet to
    // the NPC entity when it is first spawned.
    float minimum_magnitude_of_npc_entity_initial_velocity = 1000.0F;

    // The maximum distance of the NPC entities from the planet when they are
    // spawned, measured from the surface of the planet to the center of the NPC
    // entity.
    float maximum_distance_of_npc_entities_from_planet = 70000.0F;

    // The minimum distance of the NPC entities from the planet when they are
    // spawned, measured from the surface of the planet to the center of the NPC
    // entity.
    float minimum_distance_of_npc_entities_from_planet = 7500.0F;

    // The maximum angular velocity of the NPC entities when they are spawned,
    // measured in radians per second.
    float maximum_angular_velocity_of_npc_entities = 1.0F;

    // The minimum angular velocity of the NPC entities when they are spawned,
    // measured in radians per second.
    float minimum_angular_velocity_of_npc_entities = -1.0F;

    // A list of all non-planet, non-player entities in the game.
    std::vector<svanes::Entity> non_planet_non_player_entities = {};

    // A list of all collidable entities in the game.
    std::vector<svanes::Entity> collidable_entities = {};

    std::vector<svanes::Entity> collision_flashes = {};

    /**
     * Creates num_npc_entities_to_spawn random non-player, non-planet entities
     * inside the non_planet_non_player_entities vector. It will add entities
     * in the order of box, circle, triangle, and repeat.
     *
     * Additionally, it will uniformly distribute the entities in a circle
     * around the planet, with their height above the planet's surface being
     * uniformly distributed between
     * minimum_distance_of_npc_entities_from_planet and
     * maximum_distance_of_npc_entities_from_planet, while their initial
     * velocity will be tangent to the vector from the planet to the entity,
     * with its magnitude being uniformly distributed between
     * minimum_magnitude_of_npc_entity_initial_velocity and
     * maximum_magnitude_of_npc_entity_initial_velocity.
     *
     * @param world The registry to create the entities in.
     */
    void CreateNonPlayerNonPlanetEntities(svanes::Registry &world);
    bool should_quit = false;
};
