#pragma once

/**
 * Tag component marking an entity as immovable world geometry. The goose only
 * depenetrates from entities carrying this, so it goes on the ground and the
 * surrounding walls, but not on enemies or bullets.
 */
struct Solid {};

/**
 * Hit points for an entity that can be damaged and destroyed.
 *
 * FIELDS:
 * - current: Remaining hit points. The entity is destroyed once this reaches zero.
 * - max: Hit points the entity spawned with.
 */
struct Health {
    float current = 1.0F;
    float max = 1.0F;
};
