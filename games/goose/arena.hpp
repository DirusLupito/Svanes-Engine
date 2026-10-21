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
 * Builds the ground and the walls closing off the other three sides. Each piece
 * is drawn as a colored rectangle, has a collider, and is tagged Solid. The
 * server and every client build the same arena, so it is never sent.
 *
 * @param world The registry to build it in.
 */
void CreateArena(svanes::Registry &world);
