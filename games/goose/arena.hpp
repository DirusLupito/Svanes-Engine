#pragma once

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
 * is killed. It is not Solid, so geese move into it rather than being pushed
 * out first.
 */
struct DeathZone {};

/**
 * Tag component marking an entity as a place a goose can appear. Its Transform
 * is where the goose's center is placed.
 */
struct SpawnPoint {};

/**
 * Builds the ground and the walls closing off the other three sides. Each piece
 * is drawn as a colored rectangle, has a collider, and is tagged Solid, except
 * the bottom of the left wall, which is a red DeathZone instead. The server and
 * every client build the same arena, so it is never sent.
 *
 * @param world The registry to build it in.
 */
void CreateArena(svanes::Registry &world);

/**
 * Builds the spawn points resting on the ground, each tagged SpawnPoint and
 * drawn as a goose sized purple outline. Like the arena, the server and every
 * client build the same ones, so they are never sent.
 *
 * @param world The registry to build them in.
 */
void CreateSpawnPoints(svanes::Registry &world);
