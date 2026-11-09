#pragma once

#include "protocol.hpp"

#include <svanes/entity.hpp>
#include <svanes/events/event.hpp>
#include <svanes/network/clientserver/network_server.hpp>

#include <memory>
#include <string_view>

namespace svanes {
class EventManager;
class Registry;
} // namespace svanes

/**
 * One input a client sent, raised on the server at the start of the physics
 * step after it arrived.
 *
 * FIELDS:
 * - client: The client that sent it.
 * - input: The decoded input.
 */
class GooseInputEvent final : public svanes::EventOf<GooseInputEvent> {
public:
    static constexpr std::string_view EventName = "goose.input";

    /**
     * @param client The client that sent the input.
     * @param input The decoded input.
     */
    GooseInputEvent(svanes::ClientId client, PlayerInput input);

    svanes::ClientId client;
    PlayerInput input;
};

/**
 * A goose, the enemy, or a minion losing the last of its health on the
 * server. Raised by whatever dealt the lethal damage, and handled by the owner
 * of what died.
 *
 * FIELDS:
 * - entity: The entity that died.
 * - report: What died, where, and to what.
 */
class DeathEvent final : public svanes::EventOf<DeathEvent> {
public:
    static constexpr std::string_view EventName = "goose.death";

    /**
     * @param entity The entity that died.
     * @param report What died, where, and to what.
     */
    DeathEvent(svanes::Entity entity, DeathReport report);

    svanes::Entity entity;
    DeathReport report;
};

/**
 * A goose or a minion being spawned on the server. Raised when a player joins
 * or asks to respawn, and when the enemy drops a minion, and handled by the
 * owner of what spawns.
 *
 * FIELDS:
 * - entity: The goose's entity, or for a minion, the entity spawning it.
 * - report: What spawns, and where.
 */
class SpawnEvent final : public svanes::EventOf<SpawnEvent> {
public:
    static constexpr std::string_view EventName = "goose.spawn";

    /**
     * @param entity The goose's entity, or for a minion, the entity spawning
     * it.
     * @param report What spawns, and where.
     */
    SpawnEvent(svanes::Entity entity, SpawnReport report);

    svanes::Entity entity;
    SpawnReport report;
};

/**
 * Queues an event with the world clock's current time as its priority, which
 * is how every goose event is stamped.
 *
 * @param events The event manager to queue the event on.
 * @param world The registry holding the clock.
 * @param clock The entity whose Timeline is the world clock.
 * @param event The event to queue.
 */
void QueueGooseEvent(svanes::EventManager &events, const svanes::Registry &world,
                     svanes::Entity clock, std::unique_ptr<svanes::Event> event);
