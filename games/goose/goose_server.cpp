#include "arena.hpp"
#include "bullets.hpp"
#include "enemy.hpp"
#include "goose.hpp"
#include "minions.hpp"
#include "protocol.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/network/clientserver/IServer.hpp>
#include <svanes/network/clientserver/network_server.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
 */
struct Player {
    Goose goose;
    GooseIntent intent;
    ClockSetting clock;
};

/**
 * The authoritative goose world. Every client that sends input has a goose,
 * created by its first input and removed once the client disconnects. After
 * every step, each player is sent a snapshot of the world.
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
    /**
     * @param text_snapshots Whether to send snapshots as text rather than
     * binary.
     * @param benchmark_minions If given, runs a benchmark instead of the normal
     * game: regular minion groups are off, this many minions drop as soon as
     * the first player joins, and after a minute of timing every step the
     * server prints the results and quits.
     */
    GooseServer(bool text_snapshots,
                std::optional<std::uint32_t> benchmark_minions)
        : minion_spawning(!benchmark_minions), text_snapshots(text_snapshots),
          benchmark_minions(benchmark_minions) {}

    bool ShouldQuit() const override { return benchmark_done; }

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

    void Update(const svanes::ServerUpdateContext &context) override {
        for (const svanes::ClientInput &received :
             context.network.PollInputs()) {
            const PlayerInput input = DecodePlayerInput(received.message);
            Player &player = FindOrSpawn(context.world, received.client);

            const float pending_dash = player.intent.dash;
            player.intent = input.intent;
            if (player.intent.dash == 0.0F) {
                player.intent.dash = pending_dash;
            }
            player.clock = input.clock;
            if (input.minion_spawning) {
                minion_spawning = *input.minion_spawning;
            }
        }

        if (benchmark_minions && !benchmark_start && !players.empty()) {
            for (std::uint32_t index = 0; index < *benchmark_minions; ++index) {
                const float x = 2000.0F + 2000.0F * static_cast<float>(index) /
                                              static_cast<float>(
                                                  *benchmark_minions);
                SpawnMinion(context.world, enemy->GetEntity(),
                            index % 2 == 0 ? MinionKind::Melee
                                           : MinionKind::Archer,
                            {x, 700.0F});
            }
            benchmark_start = std::chrono::steady_clock::now();
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

    void PhysicsUpdate(const svanes::ServerPhysicsContext &context) override {
        const auto step_start = std::chrono::steady_clock::now();
        svanes::Registry &world = context.world;

        for (auto &[client, player] : players) {
            player.goose.Advance(world, player.intent);
            player.intent.dash = 0.0F;
        }
        enemy->Advance(world, minion_spawning);

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

        for (const BulletHit &hit : UpdateBullets(world)) {
            if (hit.target == enemy->GetEntity()) {
                enemy->Hit(world);
            }

            const bool from_enemy = hit.owner == enemy->GetEntity();
            const std::uint32_t damage =
                from_enemy ? kEnemyBulletDamage : kPlayerBulletDamage;
            const HitSource source =
                from_enemy ? HitSource::Enemy : HitSource::Player;
            if (world.HasComponent<Minion>(hit.target)) {
                DamageMinion(world, hit.target, damage);
            }
            for (auto &[client, player] : players) {
                if (hit.target == player.goose.GetEntity()) {
                    player.goose.Hit(world, hit.direction,
                                     kBulletKnockbackSpeed, damage, source);
                }
            }
        }

        if (enemy->IsAlive()) {
            KnockBackGeeseTouchingEnemy(world);
        }
        HurtGeeseTouchingMinions(world);

        ApplyDeathZones(world);

        const svanes::NetworkMessage snapshot =
            text_snapshots ? EncodeSnapshotText(TakeSnapshot(world))
                           : EncodeSnapshot(TakeSnapshot(world));
        for (const auto &[client, player] : players) {
            context.network.Send(client, snapshot);
        }

        if (benchmark_start && !benchmark_done) {
            const auto step_end = std::chrono::steady_clock::now();
            const double micros =
                std::chrono::duration<double, std::micro>(step_end - step_start)
                    .count();
            ++benchmark_steps;
            benchmark_micros += micros;
            benchmark_micros_squared += micros * micros;
            benchmark_bytes += snapshot.bytes.size();
            if (step_end - *benchmark_start >= std::chrono::seconds(60)) {
                PrintBenchmark();
                benchmark_done = true;
            }
        }
    }

private:
    /** Prints the benchmark's step timings and snapshot sizes. */
    void PrintBenchmark() const {
        const double steps = static_cast<double>(benchmark_steps);
        const double mean = benchmark_micros / steps;
        const double variance = benchmark_micros_squared / steps - mean * mean;
        std::cout << "BENCHMARK format=" << (text_snapshots ? "text" : "binary")
                  << " minions=" << *benchmark_minions
                  << " steps=" << benchmark_steps
                  << " total_ms=" << benchmark_micros / 1000.0
                  << " mean_step_us=" << mean
                  << " variance_us2=" << variance
                  << " stddev_us=" << std::sqrt(variance)
                  << " mean_snapshot_bytes="
                  << static_cast<double>(benchmark_bytes) / steps << '\n';
    }

    /**
     * @param world The registry to spawn a goose in.
     * @param client The client an input arrived from.
     * @return The client's player, spawning its goose if it has none.
     * @throws std::logic_error if the world has no spawn points.
     */
    Player &FindOrSpawn(svanes::Registry &world, svanes::ClientId client) {
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
        const Player player{Goose(world, clock, spawn), {}, {}};
        return players.emplace(client, player).first->second;
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
     * Throws every goose overlapping the living enemy away from it.
     *
     * @param world The registry containing the geese and the enemy.
     */
    void KnockBackGeeseTouchingEnemy(svanes::Registry &world) {
        const svanes::Entity enemy_entity = enemy->GetEntity();
        for (auto &[client, player] : players) {
            const svanes::Entity goose_entity = player.goose.GetEntity();
            const auto contacts = svanes::DetectCollisions(
                world.GetComponent<svanes::Collider2D>(goose_entity).geometry,
                world.GetComponent<svanes::Transform>(goose_entity),
                world.GetComponent<svanes::Collider2D>(enemy_entity).geometry,
                world.GetComponent<svanes::Transform>(enemy_entity));
            if (!contacts.empty()) {
                player.goose.Hit(world, contacts.front().normal,
                                 kEnemyKnockbackSpeed, 0, HitSource::Enemy);
            }
        }
    }

    /**
     * Kills every goose and destroys every minion overlapping a death zone.
     *
     * @param world The registry containing the geese, minions, and death
     * zones.
     */
    void ApplyDeathZones(svanes::Registry &world) {
        std::vector<svanes::Entity> doomed_minions;
        world.ForEach<DeathZone, svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity, const DeathZone &,
                const svanes::Transform &zone_transform,
                const svanes::Collider2D &zone_collider) {
                for (auto &[client, player] : players) {
                    if (Touches(world, player.goose.GetEntity(),
                                zone_transform, zone_collider)) {
                        player.goose.Kill();
                    }
                }

                world.ForEach<Minion>(
                    [&](svanes::Entity minion, const Minion &) {
                        if (Touches(world, minion, zone_transform,
                                    zone_collider)) {
                            doomed_minions.push_back(minion);
                        }
                    });
            });

        for (const svanes::Entity minion : doomed_minions) {
            if (world.HasComponent<Minion>(minion)) {
                world.DestroyEntity(minion);
            }
        }
    }

    /**
     * Hurts every living goose touching a melee minion, throwing it away from
     * the minion.
     *
     * @param world The registry containing the geese and the minions.
     */
    void HurtGeeseTouchingMinions(svanes::Registry &world) {
        world.ForEach<Minion, svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity, const Minion &minion,
                const svanes::Transform &minion_transform,
                const svanes::Collider2D &minion_collider) {
                if (minion.kind != MinionKind::Melee) {
                    return;
                }
                for (auto &[client, player] : players) {
                    const svanes::Entity goose_entity =
                        player.goose.GetEntity();
                    const auto contacts = svanes::DetectCollisions(
                        world.GetComponent<svanes::Collider2D>(goose_entity)
                            .geometry,
                        world.GetComponent<svanes::Transform>(goose_entity),
                        minion_collider.geometry, minion_transform);
                    if (!contacts.empty()) {
                        player.goose.Hit(world, contacts.front().normal,
                                         kMinionKnockbackSpeed, kMinionDamage,
                                         HitSource::Enemy);
                    }
                }
            });
    }

    /**
     * @param world The registry containing the entity.
     * @param entity An entity with a Transform and Collider2D.
     * @param other_transform Where the other shape is.
     * @param other_collider The other shape.
     * @return Whether the entity overlaps the other shape.
     */
    static bool Touches(const svanes::Registry &world, svanes::Entity entity,
                        const svanes::Transform &other_transform,
                        const svanes::Collider2D &other_collider) {
        return !svanes::DetectCollisions(
                    world.GetComponent<svanes::Collider2D>(entity).geometry,
                    world.GetComponent<svanes::Transform>(entity),
                    other_collider.geometry, other_transform)
                    .empty();
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
    bool minion_spawning;
    std::optional<Enemy> enemy;
    std::unordered_map<svanes::ClientId, Player> players;

    bool text_snapshots;
    std::optional<std::uint32_t> benchmark_minions;
    std::optional<std::chrono::steady_clock::time_point> benchmark_start;
    bool benchmark_done = false;
    std::uint64_t benchmark_steps = 0;
    double benchmark_micros = 0.0;
    double benchmark_micros_squared = 0.0;
    std::uint64_t benchmark_bytes = 0;
};

} // namespace

int32_t main(int32_t argc, char **argv) {
    bool text_snapshots = false;
    std::optional<std::uint32_t> benchmark_minions;
    try {
        for (int32_t index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--text") {
                text_snapshots = true;
            } else {
                benchmark_minions =
                    static_cast<std::uint32_t>(std::stoul(argument));
            }
        }
    } catch (const std::exception &) {
        std::cerr << "Usage: svanes_game_goose_server [--text] [MINIONS]\n"
                  << "Listens on UDP port " << kPort
                  << ". MINIONS runs a one minute benchmark.\n";
        return 1;
    }

    try {
        svanes::NetworkServer network;
        GooseServer server(text_snapshots, benchmark_minions);
        return network.run(server);
    } catch (const std::exception &error) {
        std::cerr << "Goose server: " << error.what() << '\n';
        return 1;
    }
}
