#include "arena.hpp"
#include "bullets.hpp"
#include "enemy.hpp"
#include "goose.hpp"
#include "protocol.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/network/clientserver/IServer.hpp>
#include <svanes/network/clientserver/network_server.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::uint16_t kPort = 46000;
constexpr std::chrono::seconds kPlayerTimeout{3};

constexpr float kGravity = 2000.0F;
constexpr std::array<float, 8> kSpawnX{400.0F,  550.0F,  700.0F,  850.0F,
                                       1000.0F, 1150.0F, 1300.0F, 1450.0F};
constexpr float kSpawnY = 700.0F;

constexpr float kBulletKnockbackSpeed = 350.0F;
constexpr float kEnemyKnockbackSpeed = 550.0F;

/**
 * One client's goose and what that client last asked for.
 *
 * FIELDS:
 * - goose: The client's goose.
 * - intent: The latest intent the client sent, used for every step until
 *   replaced. Its dash is kept until a step uses it, even if later messages
 *   carry no dash.
 * - clock: How the client last asked the world's clock to run.
 * - last_heard: When the client's latest input arrived.
 */
struct Player {
    Goose goose;
    GooseIntent intent;
    ClockSetting clock;
    std::chrono::steady_clock::time_point last_heard;
};

/**
 * The authoritative goose world. Every client that sends input has a goose,
 * created by its first input and removed once it goes silent. After every step,
 * each player is sent a snapshot of the world.
 *
 * Everything that moves follows one world clock. While exactly one player is
 * in the world, that player's pause and speed settings drive the clock;
 * otherwise it runs at 1x, so no player can pause or change the pace of the
 * world for anyone else.
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
        enemy.emplace(context.world, clock);

        context.network.Listen("*:" + std::to_string(kPort));
        std::cout << "Goose server listening on UDP port " << kPort << ".\n";
    }

    svanes::JoinResponse
    HandleJoinRequest(const svanes::ServerJoinContext &context) override {
        std::cout << "Client " << context.client << " connected.\n";
        return {true, {}};
    }

    void Update(const svanes::ServerUpdateContext &context) override {
        const auto now = std::chrono::steady_clock::now();

        for (const svanes::ClientInput &received :
             context.network.PollInputs()) {
            const PlayerInput input = DecodePlayerInput(received.message);
            if (input.leaving) {
                if (players.contains(received.client)) {
                    RemovePlayer(context.world, received.client);
                    std::cout << "Player " << received.client << " left.\n";
                }
                continue;
            }
            Player &player = FindOrSpawn(context.world, received.client);

            const float pending_dash = player.intent.dash;
            player.intent = input.intent;
            if (player.intent.dash == 0.0F) {
                player.intent.dash = pending_dash;
            }
            player.clock = input.clock;
            player.last_heard = now;
        }

        std::vector<svanes::ClientId> silent;
        for (const auto &[client, player] : players) {
            if (now - player.last_heard > kPlayerTimeout) {
                silent.push_back(client);
            }
        }
        for (const svanes::ClientId client : silent) {
            RemovePlayer(context.world, client);
            std::cout << "Player " << client << " timed out.\n";
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
        svanes::Registry &world = context.world;

        for (auto &[client, player] : players) {
            player.goose.Advance(world, player.intent);
            player.intent.dash = 0.0F;
        }
        enemy->Advance(world);

        for (const BulletHit &hit : UpdateBullets(world)) {
            if (hit.target == enemy->GetEntity()) {
                enemy->Hit(world);
            }
            for (auto &[client, player] : players) {
                if (hit.target == player.goose.GetEntity()) {
                    player.goose.ApplyKnockback(world, hit.direction,
                                                kBulletKnockbackSpeed);
                }
            }
        }

        if (enemy->IsAlive()) {
            KnockBackGeeseTouchingEnemy(world);
        }

        const svanes::NetworkMessage snapshot =
            EncodeSnapshot(TakeSnapshot(world));
        for (const auto &[client, player] : players) {
            context.network.Send(client, snapshot);
        }
    }

private:
    /**
     * @param world The registry to spawn a goose in.
     * @param client The client an input arrived from.
     * @return The client's player, spawning its goose if it has none.
     */
    Player &FindOrSpawn(svanes::Registry &world, svanes::ClientId client) {
        const auto found = players.find(client);
        if (found != players.end()) {
            return found->second;
        }

        const svanes::Vector2D spawn{kSpawnX[client % kSpawnX.size()], kSpawnY};
        std::cout << "Player " << client << " joined.\n";
        const Player player{Goose(world, clock, spawn), {}, {}, {}};
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
                player.goose.ApplyKnockback(world, contacts.front().normal,
                                            kEnemyKnockbackSpeed);
            }
        }
    }

    /**
     * @param world The registry containing the world.
     * @return What every player needs to draw the world.
     */
    Snapshot TakeSnapshot(const svanes::Registry &world) const {
        Snapshot snapshot{};
        snapshot.clock = clock_setting;

        for (const auto &[client, player] : players) {
            const svanes::Transform &transform =
                world.GetComponent<svanes::Transform>(player.goose.GetEntity());
            snapshot.geese.push_back(
                {client, {transform.x, transform.y}, player.goose.GetState()});
        }

        if (enemy->IsAlive()) {
            const svanes::Transform &transform =
                world.GetComponent<svanes::Transform>(enemy->GetEntity());
            snapshot.enemy = svanes::Vector2D{transform.x, transform.y};
        }

        world.ForEach<Bullet, svanes::Transform>(
            [&](svanes::Entity, const Bullet &bullet,
                const svanes::Transform &transform) {
                snapshot.bullets.push_back(
                    {{transform.x, transform.y},
                     bullet.owner == enemy->GetEntity()});
            });
        return snapshot;
    }

    svanes::Entity clock = 0;
    ClockSetting clock_setting;
    std::optional<Enemy> enemy;
    std::unordered_map<svanes::ClientId, Player> players;
};

} // namespace

int32_t main(int32_t argc, char **) {
    if (argc > 1) {
        std::cerr << "The goose server takes no arguments. It listens on UDP "
                     "port "
                  << kPort << ".\n";
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
