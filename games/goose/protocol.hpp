#pragma once

#include "goose.hpp"

#include <svanes/network/clientserver/network_server.hpp>
#include <svanes/network/network_message.hpp>
#include <svanes/utility/rational_number.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <optional>
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
 * - leaving: Whether the player is quitting. The server removes their goose
 *   as soon as it reads this, instead of waiting for them to time out.
 */
struct PlayerInput {
    GooseIntent intent;
    ClockSetting clock;
    bool leaving = false;
};

/**
 * One goose as the server last saw it.
 *
 * FIELDS:
 * - client: The client controlling it.
 * - position: Its world position.
 * - state: Which animation it is showing.
 */
struct SnapshotGoose {
    svanes::ClientId client;
    svanes::Vector2D position;
    GooseState state;
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
 * The only message the server sends, to every player after each step.
 *
 * FIELDS:
 * - clock: How the world's clock is running.
 * - geese: Every player's goose.
 * - enemy: The enemy's position, or empty once it is dead.
 * - bullets: Every live bullet.
 */
struct Snapshot {
    ClockSetting clock;
    std::vector<SnapshotGoose> geese;
    std::optional<svanes::Vector2D> enemy;
    std::vector<SnapshotBullet> bullets;
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
