#pragma once

#include <svanes/entity.hpp>

namespace svanes {

class Registry;

}

/**
 * Tag component marking an entity as immovable world geometry. Geese only
 * depenetrate from entities carrying this, so it goes on the ground and the
 * surrounding walls, but not on the enemy or bullets.
 */
struct Solid {};

/**
 * Tag component marking an entity as deadly to touch. A goose overlapping one
 * is killed. It is not Solid, so a goose pressed against a wall it pokes out of
 * still overlaps it.
 */
struct DeathZone {};

/**
 * Tag component marking an entity as a place a goose can appear. Its Transform
 * is where the goose's center is placed.
 */
struct SpawnPoint {};

/**
 * Builds the ground and the walls closing off the other three sides. Each piece
 * is drawn as a colored rectangle, has a collider, and is tagged Solid. The
 * server and every client build the same arena, so it is never sent.
 *
 * @param world The registry to build it in.
 */
void CreateArena(svanes::Registry &world);

/**
 * Builds the hidden spawn points resting on the ground, each tagged
 * SpawnPoint. Only the server needs them.
 *
 * @param world The registry to build them in.
 */
void CreateSpawnPoints(svanes::Registry &world);

/**
 * Builds the hidden death zone covering the bottom of the left wall, tagged
 * DeathZone. It pokes out of the wall slightly, so a goose pushed flush against
 * the wall still overlaps it. Only the server needs it.
 *
 * @param world The registry to build it in.
 */
void CreateDeathZones(svanes::Registry &world);

/**
 * Pushes an entity out of every Solid it overlaps, stopping the velocity that
 * drove it into each one.
 *
 * @param world The registry containing the entity and the arena.
 * @param entity An entity with a Transform, Kinematic2D, and Collider2D.
 * @return Whether the entity is standing on a Solid.
 */
bool ResolveSolidCollisions(svanes::Registry &world, svanes::Entity entity);
