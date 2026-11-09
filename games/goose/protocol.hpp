#pragma once

#include "goose.hpp"
#include "minions.hpp"

#include <svanes/network/clientserver/network_server.hpp>
#include <svanes/network/network_message.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/utility/rational_number.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

/**
 * The speeds the pause menu offers, slowest first.
 *
 * MEMBERS:
 * - Half: Half of real time.
 * - Normal: Real time.
 * - Double: Twice real time.
 */
enum class GooseSpeed : std::uint8_t {
    Half,
    Normal,
    Double,
};

/**
 * @param speed A speed from the pause menu.
 * @return The tic size that runs a timeline at that speed, in parent tics per
 * local tic.
 * @throws std::invalid_argument for a value outside GooseSpeed.
 */
svanes::RationalNumber TicSize(GooseSpeed speed);

/**
 * How the world's clock runs. Clients send the setting they want, and the
 * server sends the one in effect.
 *
 * FIELDS:
 * - speed: The clock's speed.
 * - paused: Whether the clock is stopped.
 */
struct ClockSetting {
    GooseSpeed speed = GooseSpeed::Normal;
    bool paused = false;
};

/**
 * The only message a client sends, once per frame.
 *
 * FIELDS:
 * - intent: What the player wants their goose to do.
 * - clock: How the player wants the world's clock to run. The server follows
 *   it only while this player is alone.
 * - minion_spawning: Whether the player wants the enemy to drop minions, or
 *   empty if they have not asked for a change. Any player may change it for
 *   everyone.
 * - respawn: Whether the player wants their dead goose spawned again. Sent
 *   every frame until it takes effect, so a lost message only delays it.
 *   Ignored while the goose is alive.
 */
struct PlayerInput {
    GooseIntent intent;
    ClockSetting clock;
    std::optional<bool> minion_spawning;
    bool respawn = false;
};

/**
 * What a death or spawn report is about.
 *
 * MEMBERS:
 * - Goose: A player's goose.
 * - Enemy: The enemy.
 * - Minion: One of the enemy's minions.
 */
enum class Subject : std::uint8_t {
    Goose,
    Enemy,
    Minion,
};

/**
 * What took the last of something's health.
 *
 * MEMBERS:
 * - EnemyFire: A bullet from the enemy or one of its archers.
 * - PlayerFire: A bullet from a goose.
 * - Minion: A melee minion's touch.
 * - DeathZone: Falling into a death zone.
 */
enum class DeathCause : std::uint8_t {
    EnemyFire,
    PlayerFire,
    Minion,
    DeathZone,
};

/**
 * Something that died on the server.
 *
 * FIELDS:
 * - subject: What died.
 * - client: The client whose goose died, or 0 if subject is not Goose.
 * - position: Where it died.
 * - cause: What killed it.
 * - killer: The client whose goose fired the killing bullet, or 0 if cause is
 *   not PlayerFire or that client has left.
 */
struct DeathReport {
    Subject subject;
    svanes::ClientId client;
    svanes::Vector2D position;
    DeathCause cause;
    svanes::ClientId killer;
};

/**
 * Something that spawned on the server.
 *
 * FIELDS:
 * - subject: What spawned. Never Enemy, which exists from the start.
 * - client: The client whose goose spawned, or 0 if subject is not Goose.
 * - minion_kind: How the minion fights. Only meaningful if subject is Minion.
 * - position: Where it spawned.
 */
struct SpawnReport {
    Subject subject;
    svanes::ClientId client;
    MinionKind minion_kind;
    svanes::Vector2D position;
};

/**
 * How many steps the server keeps sending each death and spawn report for, so
 * a client that loses some snapshots still hears about it.
 */
inline constexpr std::uint64_t ReportSteps = 50;

/**
 * A death or spawn as the server sends it, in every snapshot for ReportSteps
 * steps after it happened.
 *
 * FIELDS:
 * - sequence: Counts every report the server makes, starting at 1, so a client
 *   can skip the ones it has already seen.
 * - time: The world clock's time when it happened, in tics.
 * - report: What happened.
 */
struct SequencedReport {
    std::uint32_t sequence;
    svanes::TicCount time;
    std::variant<DeathReport, SpawnReport> report;
};

/**
 * One goose as the server last saw it.
 *
 * FIELDS:
 * - client: The client controlling it.
 * - position: Its world position.
 * - state: Which animation it is showing.
 * - health: Its health, from zero up to GooseMaxHealth.
 */
struct SnapshotGoose {
    svanes::ClientId client;
    svanes::Vector2D position;
    GooseState state;
    std::uint32_t health;
};

/**
 * One bullet as the server last saw it.
 *
 * FIELDS:
 * - position: Its world position.
 * - from_enemy: Whether the enemy fired it rather than a goose.
 */
struct SnapshotBullet {
    svanes::Vector2D position;
    bool from_enemy;
};

/**
 * The enemy as the server last saw it.
 *
 * FIELDS:
 * - position: Its world position.
 * - health: Its health, from zero up to EnemyMaxHealth.
 */
struct SnapshotEnemy {
    svanes::Vector2D position;
    std::uint32_t health;
};

/**
 * One minion as the server last saw it.
 *
 * FIELDS:
 * - position: Its world position.
 * - kind: How it fights, which decides how it is drawn.
 * - health: Its health, from one up to MinionMaxHealth.
 */
struct SnapshotMinion {
    svanes::Vector2D position;
    MinionKind kind;
    std::uint32_t health;
};

/**
 * The only message the server sends, to every player after each step.
 *
 * FIELDS:
 * - clock: How the world's clock is running.
 * - geese: Every player's goose.
 * - enemy: The enemy, or empty once it is dead.
 * - bullets: Every live bullet.
 * - minions: Every live minion.
 * - minion_spawning: Whether the enemy is allowed to drop minions.
 * - reports: Every death and spawn from the last ReportSteps steps, oldest
 *   first.
 */
struct Snapshot {
    ClockSetting clock;
    std::vector<SnapshotGoose> geese;
    std::optional<SnapshotEnemy> enemy;
    std::vector<SnapshotBullet> bullets;
    std::vector<SnapshotMinion> minions;
    bool minion_spawning = true;
    std::vector<SequencedReport> reports;
};

/**
 * @param input The player's input for this frame.
 * @return The input as a message.
 */
svanes::NetworkMessage EncodePlayerInput(const PlayerInput &input);

/**
 * @param message A message written by EncodePlayerInput.
 * @return The decoded input.
 * @throws std::invalid_argument if the message is too short or too long.
 */
PlayerInput DecodePlayerInput(const svanes::NetworkMessage &message);

/**
 * @param snapshot The world after a step.
 * @return The snapshot as a message.
 */
svanes::NetworkMessage EncodeSnapshot(const Snapshot &snapshot);

/**
 * @param message A message written by EncodeSnapshot.
 * @return The decoded snapshot.
 * @throws std::invalid_argument if the message is too short or too long.
 */
Snapshot DecodeSnapshot(const svanes::NetworkMessage &message);
