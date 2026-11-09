#include "arena.hpp"
#include "bullets.hpp"
#include "enemy.hpp"
#include "goose.hpp"
#include "goose_events.hpp"
#include "minions.hpp"
#include "protocol.hpp"

#include <svanes/collision_pass.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/events/collision_event.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/network/clientserver/IServer.hpp>
#include <svanes/network/clientserver/network_server.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr std::uint16_t kPort = 46000;

constexpr float kGravity = 2000.0F;

constexpr float kBulletKnockbackSpeed = 350.0F;
constexpr float kEnemyKnockbackSpeed = 550.0F;
constexpr std::uint32_t kEnemyBulletDamage = 1;
constexpr std::uint32_t kPlayerBulletDamage = 1;
constexpr std::uint32_t kMinionDamage = 1;
constexpr float kMinionKnockbackSpeed = 400.0F;

/**
 * One client's goose and what that client last asked for.
 *
 * FIELDS:
 * - goose: The client's goose.
 * - intent: The latest intent the client sent, used for every step until
 *   replaced. Its dash is kept until a step uses it, even if later messages
 *   carry no dash.
 * - clock: How the client last asked the world's clock to run.
 * - spawning: Whether a SpawnEvent for the goose is queued and not yet
 *   handled, so several respawn requests in one step spawn it only once.
 */
struct Player {
    Goose goose;
    GooseIntent intent;
    ClockSetting clock;
    bool spawning = false;
};

/**
 * What an entity with a collider is to goose's contact rules. The order
 * matters: a collision's two sides are sorted by role, so each rule only has
 * to match one arrangement of its pair.
 *
 * MEMBERS:
 * - Bullet: Any bullet.
 * - Goose: A player's goose.
 * - Enemy: The enemy.
 * - Minion: One of the enemy's minions.
 * - DeathZone: A death zone.
 * - Solid: The arena's ground and walls.
 */
enum class Role : std::uint8_t {
    Bullet,
    Goose,
    Enemy,
    Minion,
    DeathZone,
    Solid,
};

/**
 * One side of a collision, as goose's contact rules see it.
 *
 * FIELDS:
 * - entity: The entity on this side.
 * - role: What the entity is.
 */
struct Contact {
    svanes::Entity entity;
    Role role;
};

/**
 * @param collision A collision reported by the engine.
 * @param entity One of the collision's two entities.
 * @return The direction that moves the entity out of the other one.
 */
svanes::Vector2D PushOut(const svanes::EntityCollision2D &collision,
                         svanes::Entity entity) {
    const svanes::Vector2D normal = collision.collisions.front().normal;
    if (entity == collision.a) {
        return normal;
    }
    return {-normal.x, -normal.y};
}

/**
 * The authoritative goose world. Every client that sends input has a goose,
 * created by its first input and removed once the client disconnects. After
 * every step, each player is sent a snapshot of the world.
 *
 * Gameplay runs on events. Each input a client sends becomes a
 * GooseInputEvent, every overlap the engine finds becomes a CollisionEvent
 * handled by goose's contact rules, and those rules raise a DeathEvent when
 * they take the last of something's health. Joining, respawning, and the
 * enemy dropping minions raise SpawnEvents. Each of these is handled by
 * whatever owns the thing it is about.
 *
 * Everything that moves follows one world clock. While exactly one player is
 * in the world, that player's pause and speed settings drive the clock;
 * otherwise it runs at 1x, so no player can pause or change the pace of the
 * world for anyone else.
 *
 * Whether the enemy drops minions is shared differently: any player can turn
 * it on or off for everyone.
 */
class GooseServer final : public svanes::IServer {
public:
    void Initialize(svanes::ServerContext &context) override {
        // the world gravity vector, applied by the engine every frame to any entity
        // holding both Kinematic2D and Gravity. Positive y is down
        context.gravity = {0.0F,
                           svanes::PerSecondSquaredToPerTicSquared(kGravity)};

        clock = context.world.CreateEntity();
        context.world.AddComponent<svanes::Timeline>(clock);
        CreateArena(context.world);
        CreateSpawnPoints(context.world);
        CreateDeathZones(context.world);
        enemy.emplace(context.world, clock);

        Subscribe(context.world, context.events);

        context.network.Listen("*:" + std::to_string(kPort));
        std::cout << "Goose server listening on UDP port " << kPort << ".\n";
    }

    svanes::JoinResponse
    HandleJoinRequest(const svanes::ServerJoinContext &context) override {
        std::cout << "Client " << context.client << " connected.\n";
        return {true, {}};
    }

    void HandleDisconnect(svanes::Registry &world,
                          svanes::ClientId client) override {
        if (players.contains(client)) {
            RemovePlayer(world, client);
        }
        std::cout << "Player " << client << " left.\n";
    }

    /**
     * Applies the clock and minion spawning settings from each input right
     * away, since the clock has to be set before the engine advances it, and
     * holds each input for PhysicsUpdate to raise as a GooseInputEvent.
     */
    void Update(const svanes::ServerUpdateContext &context) override {
        for (const svanes::ClientInput &received :
             context.network.PollInputs()) {
            PlayerInput input = DecodePlayerInput(received.message);
            const auto found = players.find(received.client);
            if (found != players.end()) {
                found->second.clock = input.clock;
            }
            if (input.minion_spawning) {
                minion_spawning = *input.minion_spawning;
            }
            inputs.push_back(std::make_unique<GooseInputEvent>(
                received.client, std::move(input)));
        }

        clock_setting = ClockSetting{};
        if (players.size() == 1) {
            clock_setting = players.begin()->second.clock;
        }

        svanes::Timeline &timeline =
            context.world.GetComponent<svanes::Timeline>(clock);
        timeline.SetTicSize(TicSize(clock_setting.speed));
        if (clock_setting.paused) {
            timeline.Pause();
        } else {
            timeline.Unpause();
        }
    }

    /**
     * Runs one step. The second dispatch exists because the contact rules
     * raise deaths during the first, and an event raised during a dispatch
     * waits for the next one. Handling every death after every collision means
     * no collision handler sees an entity a death has already destroyed.
     */
    void PhysicsUpdate(const svanes::ServerPhysicsContext &context) override {
        svanes::Registry &world = context.world;
        svanes::EventManager &events = context.events;

        for (std::unique_ptr<GooseInputEvent> &input : inputs) {
            QueueGooseEvent(events, world, clock, std::move(input));
        }
        inputs.clear();
        events.DispatchEvents();

        for (auto &[client, player] : players) {
            player.goose.Advance(world, player.intent);
            player.intent.dash = 0.0F;
        }
        enemy->Advance(world, events, minion_spawning);

        std::vector<svanes::Vector2D> living_geese;
        for (const auto &[client, player] : players) {
            if (!player.goose.IsDead()) {
                const svanes::Transform &transform =
                    world.GetComponent<svanes::Transform>(
                        player.goose.GetEntity());
                living_geese.push_back({transform.x, transform.y});
            }
        }
        AdvanceMinions(world, living_geese);

        std::vector<svanes::Entity> colliders;
        world.ForEach<svanes::Collider2D>(
            [&](svanes::Entity entity, const svanes::Collider2D &) {
                colliders.push_back(entity);
            });
        svanes::DetectEntityCollisions(world, colliders, events,
                                       context.parallel_for);
        events.DispatchEvents();
        events.DispatchEvents();

        for (const svanes::Entity bullet : spent_bullets) {
            world.DestroyEntity(bullet);
        }
        spent_bullets.clear();

        const svanes::NetworkMessage snapshot =
            EncodeSnapshot(TakeSnapshot(world));
        for (const auto &[client, player] : players) {
            context.network.Send(client, snapshot);
        }
    }

private:
    /**
     * Registers the server's handler for each kind of event it raises or
     * receives.
     *
     * @param world The registry the handlers act on.
     * @param events The event manager to register them with.
     */
    void Subscribe(svanes::Registry &world, svanes::EventManager &events) {
        events.Subscribe(
            std::make_unique<svanes::EventHandlerWrapper<GooseInputEvent>>(
                [this, &world, &events](const GooseInputEvent &event) {
                    HandleInput(world, events, event);
                }));
        events.Subscribe(
            std::make_unique<
                svanes::EventHandlerWrapper<svanes::CollisionEvent>>(
                [this, &world, &events](const svanes::CollisionEvent &event) {
                    HandleCollision(world, events, event);
                }));
        events.Subscribe(
            std::make_unique<svanes::EventHandlerWrapper<DeathEvent>>(
                [this, &world](const DeathEvent &event) {
                    HandleDeath(world, event);
                }));
        events.Subscribe(
            std::make_unique<svanes::EventHandlerWrapper<SpawnEvent>>(
                [this, &world](const SpawnEvent &event) {
                    HandleSpawn(world, event);
                }));
    }

    /**
     * Gives a client's goose its latest intent, joining the client if this is
     * its first input, and raises a spawn if a dead goose asks to respawn.
     *
     * @param world The registry holding the geese.
     * @param events The event manager to raise spawns on.
     * @param event The input.
     */
    void HandleInput(svanes::Registry &world, svanes::EventManager &events,
                     const GooseInputEvent &event) {
        Player &player = FindOrJoin(world, events, event.client);

        const float pending_dash = player.intent.dash;
        player.intent = event.input.intent;
        if (player.intent.dash == 0.0F) {
            player.intent.dash = pending_dash;
        }

        if (event.input.respawn && player.goose.IsDead() && !player.spawning) {
            QueueGooseSpawn(world, events, event.client, player);
        }
    }

    /**
     * Applies goose's contact rules to one collision: bullets strike what they
     * do not pass through, the enemy knocks geese back, melee minions hurt
     * geese, and death zones kill geese and minions. Every other pair is
     * ignored, including geese and minions on the arena's Solids, which
     * ResolveSolidCollisions handles directly.
     *
     * @param world The registry holding both entities.
     * @param events The event manager to raise deaths on.
     * @param event The collision.
     */
    void HandleCollision(svanes::Registry &world, svanes::EventManager &events,
                         const svanes::CollisionEvent &event) {
        const svanes::EntityCollision2D &collision = event.collision;
        Contact first{collision.a, RoleOf(world, collision.a)};
        Contact second{collision.b, RoleOf(world, collision.b)};
        if (second.role < first.role) {
            std::swap(first, second);
        }

        if (first.role == Role::Bullet) {
            StrikeWithBullet(world, events, first.entity, second);
        } else if (first.role == Role::Goose && second.role == Role::Enemy) {
            GooseOf(first.entity).Hit(world, PushOut(collision, first.entity),
                                      kEnemyKnockbackSpeed, 0,
                                      HitSource::Enemy);
        } else if (first.role == Role::Goose && second.role == Role::Minion) {
            const Minion &minion = world.GetComponent<Minion>(second.entity);
            if (minion.kind == MinionKind::Melee && minion.health > 0 &&
                GooseOf(first.entity)
                    .Hit(world, PushOut(collision, first.entity),
                         kMinionKnockbackSpeed, kMinionDamage,
                         HitSource::Enemy)) {
                QueueDeath(world, events, first.entity, Subject::Goose,
                           DeathCause::Minion, 0);
            }
        } else if (first.role == Role::Goose &&
                   second.role == Role::DeathZone) {
            if (GooseOf(first.entity).Kill()) {
                QueueDeath(world, events, first.entity, Subject::Goose,
                           DeathCause::DeathZone, 0);
            }
        } else if (first.role == Role::Minion &&
                   second.role == Role::DeathZone) {
            if (DamageMinion(world, first.entity, MinionMaxHealth)) {
                QueueDeath(world, events, first.entity, Subject::Minion,
                           DeathCause::DeathZone, 0);
            }
        }
    }

    /**
     * Strikes a target with a bullet, unless the bullet passes through it or
     * already struck something this step. The bullet is destroyed after the
     * step's events are handled. Geese are knocked along the bullet's path,
     * and anything it takes the last health from dies.
     *
     * @param world The registry holding the bullet and the target.
     * @param events The event manager to raise deaths on.
     * @param bullet_entity The bullet.
     * @param target What the bullet overlaps.
     */
    void StrikeWithBullet(svanes::Registry &world, svanes::EventManager &events,
                          svanes::Entity bullet_entity, const Contact &target) {
        const Bullet &bullet = world.GetComponent<Bullet>(bullet_entity);
        if (spent_bullets.contains(bullet_entity) ||
            PassesThrough(world, bullet, target.entity)) {
            return;
        }
        spent_bullets.insert(bullet_entity);

        const bool from_enemy = bullet.owner == enemy->GetEntity();
        const std::uint32_t damage =
            from_enemy ? kEnemyBulletDamage : kPlayerBulletDamage;
        const DeathCause cause =
            from_enemy ? DeathCause::EnemyFire : DeathCause::PlayerFire;
        const svanes::ClientId killer =
            FindGooseOwner(bullet.owner).value_or(0);

        if (target.role == Role::Goose) {
            const svanes::Kinematic2D &motion =
                world.GetComponent<svanes::Kinematic2D>(bullet_entity);
            if (GooseOf(target.entity)
                    .Hit(world, {motion.velocity_x, motion.velocity_y},
                         kBulletKnockbackSpeed, damage,
                         from_enemy ? HitSource::Enemy : HitSource::Player)) {
                QueueDeath(world, events, target.entity, Subject::Goose, cause,
                           killer);
            }
        } else if (target.role == Role::Enemy) {
            if (enemy->Hit()) {
                QueueDeath(world, events, target.entity, Subject::Enemy, cause,
                           killer);
            }
        } else if (target.role == Role::Minion) {
            if (DamageMinion(world, target.entity, damage)) {
                QueueDeath(world, events, target.entity, Subject::Minion,
                           cause, killer);
            }
        }
    }

    /**
     * Handles a death by telling the owner of what died: the goose lies still,
     * the enemy loses its collider, and the minion is destroyed.
     *
     * @param world The registry holding what died.
     * @param event The death.
     * @throws std::invalid_argument for a subject outside Subject.
     */
    void HandleDeath(svanes::Registry &world, const DeathEvent &event) {
        switch (event.report.subject) {
        case Subject::Goose:
            players.at(event.report.client).goose.Die(world);
            return;
        case Subject::Enemy:
            enemy->Die(world);
            return;
        case Subject::Minion:
            world.DestroyEntity(event.entity);
            return;
        }
        throw std::invalid_argument("A DeathEvent had an unknown subject.");
    }

    /**
     * Handles a spawn: a goose is placed at its spawn point with full health,
     * and a minion is created where the enemy dropped it.
     *
     * @param world The registry to spawn in.
     * @param event The spawn.
     * @throws std::invalid_argument for the enemy, which is never spawned, or a
     * subject outside Subject.
     */
    void HandleSpawn(svanes::Registry &world, const SpawnEvent &event) {
        switch (event.report.subject) {
        case Subject::Goose: {
            Player &player = players.at(event.report.client);
            player.goose.Spawn(world);
            player.spawning = false;
            return;
        }
        case Subject::Minion:
            SpawnMinion(world, event.entity, event.report.minion_kind,
                        event.report.position);
            return;
        case Subject::Enemy:
            break;
        }
        throw std::invalid_argument(
            "A SpawnEvent had a subject that is never spawned.");
    }

    /**
     * Raises the death of something with a Transform, at where it is now.
     *
     * @param world The registry holding what died.
     * @param events The event manager to raise the death on.
     * @param entity The entity that died.
     * @param subject What died.
     * @param cause What killed it.
     * @param killer The client whose goose fired the killing bullet, or 0.
     */
    void QueueDeath(svanes::Registry &world, svanes::EventManager &events,
                    svanes::Entity entity, Subject subject, DeathCause cause,
                    svanes::ClientId killer) {
        const svanes::Transform &transform =
            world.GetComponent<svanes::Transform>(entity);
        const svanes::ClientId client =
            subject == Subject::Goose ? FindGooseOwner(entity).value() : 0;
        QueueGooseEvent(
            events, world, clock,
            std::make_unique<DeathEvent>(
                entity, DeathReport{subject,
                                    client,
                                    {transform.x, transform.y},
                                    cause,
                                    killer}));
    }

    /**
     * Raises the spawn of a player's goose at its spawn point.
     *
     * @param world The registry holding the clock.
     * @param events The event manager to raise the spawn on.
     * @param client The player's client.
     * @param player The player.
     */
    void QueueGooseSpawn(svanes::Registry &world, svanes::EventManager &events,
                         svanes::ClientId client, Player &player) {
        player.spawning = true;
        QueueGooseEvent(events, world, clock,
                        std::make_unique<SpawnEvent>(
                            player.goose.GetEntity(),
                            SpawnReport{Subject::Goose, client,
                                        MinionKind::Melee,
                                        player.goose.GetSpawnPoint()}));
    }

    /**
     * @param world The registry containing the entity.
     * @param entity An entity with a collider.
     * @return What the entity is to goose's contact rules.
     * @throws std::logic_error if the entity is none of the things goose
     * gives colliders to.
     */
    Role RoleOf(const svanes::Registry &world, svanes::Entity entity) const {
        if (world.HasComponent<Bullet>(entity)) {
            return Role::Bullet;
        }
        if (FindGooseOwner(entity)) {
            return Role::Goose;
        }
        if (entity == enemy->GetEntity()) {
            return Role::Enemy;
        }
        if (world.HasComponent<Minion>(entity)) {
            return Role::Minion;
        }
        if (world.HasComponent<DeathZone>(entity)) {
            return Role::DeathZone;
        }
        if (world.HasComponent<Solid>(entity)) {
            return Role::Solid;
        }
        throw std::logic_error("Goose's contact rules found a collider on "
                               "entity " +
                               std::to_string(entity) + " with no role.");
    }

    /**
     * @param entity Any entity.
     * @return The client whose goose the entity is, or empty if it is not a
     * goose.
     */
    std::optional<svanes::ClientId>
    FindGooseOwner(svanes::Entity entity) const {
        for (const auto &[client, player] : players) {
            if (player.goose.GetEntity() == entity) {
                return client;
            }
        }
        return std::nullopt;
    }

    /**
     * @param entity A goose's entity.
     * @return The goose.
     * @throws std::bad_optional_access if the entity is not a goose.
     */
    Goose &GooseOf(svanes::Entity entity) {
        return players.at(FindGooseOwner(entity).value()).goose;
    }

    /**
     * @param world The registry to create a goose in.
     * @param events The event manager to raise the goose's spawn on.
     * @param client The client an input arrived from.
     * @return The client's player, joining it with a newly spawned goose if
     * it has none.
     * @throws std::logic_error if the world has no spawn points.
     */
    Player &FindOrJoin(svanes::Registry &world, svanes::EventManager &events,
                       svanes::ClientId client) {
        const auto found = players.find(client);
        if (found != players.end()) {
            return found->second;
        }

        std::vector<svanes::Vector2D> spawns;
        world.ForEach<SpawnPoint, svanes::Transform>(
            [&](svanes::Entity, const SpawnPoint &,
                const svanes::Transform &transform) {
                spawns.push_back({transform.x, transform.y});
            });
        if (spawns.empty()) {
            throw std::logic_error("The goose server has no spawn points.");
        }
        std::ranges::sort(spawns, {}, &svanes::Vector2D::x);

        const svanes::Vector2D spawn = spawns[(client - 1) % spawns.size()];
        std::cout << "Player " << client << " joined.\n";
        Player &player =
            players.emplace(client, Player{Goose(world, clock, spawn), {}, {}})
                .first->second;
        QueueGooseSpawn(world, events, client, player);
        return player;
    }

    /**
     * Destroys a client's goose and forgets the client.
     *
     * @param world The registry containing the goose.
     * @param client A client that currently has a goose.
     */
    void RemovePlayer(svanes::Registry &world, svanes::ClientId client) {
        world.DestroyEntity(players.at(client).goose.GetEntity());
        players.erase(client);
    }

    /**
     * @param world The registry containing the world.
     * @return What every player needs to draw the world.
     */
    Snapshot TakeSnapshot(const svanes::Registry &world) const {
        Snapshot snapshot{};
        snapshot.clock = clock_setting;
        snapshot.minion_spawning = minion_spawning;

        for (const auto &[client, player] : players) {
            const svanes::Transform &transform =
                world.GetComponent<svanes::Transform>(player.goose.GetEntity());
            snapshot.geese.push_back({client,
                                      {transform.x, transform.y},
                                      player.goose.GetState(),
                                      player.goose.GetHealth()});
        }

        if (enemy->IsAlive()) {
            const svanes::Transform &transform =
                world.GetComponent<svanes::Transform>(enemy->GetEntity());
            snapshot.enemy =
                SnapshotEnemy{{transform.x, transform.y}, enemy->GetHealth()};
        }

        world.ForEach<Bullet, svanes::Transform>(
            [&](svanes::Entity, const Bullet &bullet,
                const svanes::Transform &transform) {
                snapshot.bullets.push_back(
                    {{transform.x, transform.y},
                     bullet.owner == enemy->GetEntity()});
            });

        world.ForEach<Minion, svanes::Transform>(
            [&](svanes::Entity, const Minion &minion,
                const svanes::Transform &transform) {
                snapshot.minions.push_back(
                    {{transform.x, transform.y}, minion.kind, minion.health});
            });
        return snapshot;
    }

    svanes::Entity clock = 0;
    ClockSetting clock_setting;
    bool minion_spawning = true;
    std::optional<Enemy> enemy;
    std::unordered_map<svanes::ClientId, Player> players;
    std::vector<std::unique_ptr<GooseInputEvent>> inputs;
    std::unordered_set<svanes::Entity> spent_bullets;
};

} // namespace

int32_t main(int32_t argc, char **) {
    if (argc != 1) {
        std::cerr << "Usage: svanes_game_goose_server\n"
                  << "Listens on UDP port " << kPort << ".\n";
        return 1;
    }

    try {
        svanes::NetworkServer network;
        GooseServer server;
        return network.run(server);
    } catch (const std::exception &error) {
        std::cerr << "Goose server: " << error.what() << '\n';
        return 1;
    }
}
