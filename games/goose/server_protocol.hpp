#pragma once

#include "goose_session.hpp"
#include "goose_simulation.hpp"

#include <svanes/network/network_message.hpp>
#include <svanes/network/network_session.hpp>

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

/** The port of the server's first client slot. Slot N listens on this port plus N. */
inline constexpr std::uint16_t GooseServerFirstPort = 46000;

/** How many clients a goose server holds at once, one per slot. */
inline constexpr std::uint32_t GooseServerSlots = GooseMaxPlayers;

/** How long either side waits without hearing from the other before giving up on it. */
inline constexpr std::chrono::milliseconds GooseServerTimeout{3000};

/**
 * What a message between a goose client and server carries.
 * - Hello: A client asking a slot for a place in the world.
 * - Welcome: The slot accepted the client and names its player.
 * - Taken: The slot already belongs to another client.
 * - Input: A client's intent for the next step and its clock request.
 * - Snapshot: The server's world and who is in it.
 * - Goodbye: A client leaving.
 */
enum class GooseMessageType : std::uint8_t {
    Hello,
    Welcome,
    Taken,
    Input,
    Snapshot,
    Goodbye,
};

/**
 * A decoded message. Only the fields its type uses are filled in.
 * FIELDS:
 * - type: What the message is.
 * - client: The client's random id, for Hello, Welcome, and Taken.
 * - peer: The player the client controls, for Welcome.
 * - intent: The client's intent, for Input.
 * - clock: The client's clock request, for Input.
 * - roster: Every player in ascending id order, for Snapshot.
 * - world: Bytes from GooseSimulation::Save, for Snapshot.
 */
struct GooseMessage {
    GooseMessageType type;
    svanes::ClientId client = 0;
    svanes::PeerId peer{};
    GooseIntent intent{};
    ClockRequest clock{};
    std::vector<svanes::PeerId> roster;
    std::vector<std::byte> world;
};

/**
 * @param client The client's random id, echoed in the reply so it knows the reply is its own.
 * @return A Hello message.
 */
svanes::NetworkMessage EncodeHello(svanes::ClientId client);

/**
 * @param client The accepted client's id.
 * @param peer The player it controls.
 * @return A Welcome message.
 */
svanes::NetworkMessage EncodeWelcome(svanes::ClientId client, svanes::PeerId peer);

/**
 * @param client The refused client's id.
 * @return A Taken message.
 */
svanes::NetworkMessage EncodeTaken(svanes::ClientId client);

/**
 * @param simulation The simulation whose input encoding to use.
 * @param intent The intent for the next step.
 * @param clock What the client wants the world's clock to do.
 * @return An Input message.
 */
svanes::NetworkMessage EncodeInput(const GooseSimulation& simulation, const GooseIntent& intent, ClockRequest clock);

/**
 * @param roster Every player in ascending id order.
 * @param world Bytes from GooseSimulation::Save.
 * @return A Snapshot message.
 */
svanes::NetworkMessage EncodeSnapshot(std::span<const svanes::PeerId> roster, const svanes::NetworkMessage& world);

/** @return A Goodbye message. */
svanes::NetworkMessage EncodeGoodbye();

/**
 * @param message A message written by one of the Encode functions.
 * @param simulation The simulation whose input decoding to use.
 * @return The decoded message.
 * @throws std::invalid_argument for an unknown type or malformed contents.
 */
GooseMessage DecodeGooseMessage(const svanes::NetworkMessage& message, const GooseSimulation& simulation);
