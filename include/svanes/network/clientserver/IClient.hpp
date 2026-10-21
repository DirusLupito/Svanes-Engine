#pragma once

#include <svanes/network/network_message.hpp>
#include <svanes/timeline_system.hpp>

#include <cstdint>

namespace svanes {

class AudioManager;
class Camera2D;
class FontManager;
class InputManager;
class NetworkClient;
class Registry;
class TextureManager;

/**
 * Represents the outcome of a client's join request.
 *
 * MEMBERS:
 * - Accepted: The server admitted the client.
 * - Rejected: The server declined the client's request.
 * - TimedOut: No result was received within the requested interval.
 */
enum class JoinStatus { Accepted, Rejected, TimedOut };

/**
 * The join result, delivered once by the engine.
 *
 * FIELDS:
 * - status: Whether the join request was accepted, rejected, or timed out.
 * - client_id: The server assigned ID, or zero unless accepted.
 * - message: The game's response bytes, empty for a timeout.
 */
struct JoinResult {
    JoinStatus status;
    std::uint32_t client_id;
    NetworkMessage message;
};

/**
 * Defines relevant context for a client.
 * For clients to have their textures, audio, and fonts managed by the engine,
 * they must use the managers provided in this context.
 *
 * FIELDS:
 * - world: The engine owned registry containing the client's entities and
 * components.
 * - network: The engine owned network client used to connect to the server
 * and send/receive messages.
 * - assets: The engine owned texture manager used to load and create textures.
 * - audio: The engine owned audio manager used to load and play sounds and
 * music.
 * - fonts: The engine owned font manager used to load and render fonts.
 * - camera: The engine owned camera used to convert between screen and world
 * coordinates.
 * - output_width: The current width of the rendering output.
 * - output_height: The current height of the rendering output.
 */
struct ClientContext {
    Registry &world;
    NetworkClient &network;
    TextureManager &assets;
    AudioManager &audio;
    FontManager &fonts;
    Camera2D &camera;
    std::int32_t output_width;
    std::int32_t output_height;
};

/**
 * Defines relevant context for a single frame of a client.
 * The client uses server messages to update its view and local input to
 * prepare messages for the server. While the server owns the authoritative
 * game state, the client owns its view and controls. Additionally, the
 * client may use this data for client-side prediction or interpolation
 * for framerates that are higher than the server's simulation step rate.
 *
 * For games to have their input managed by the engine,
 * they must use the InputManager provided in this context.
 * The engine fills real_delta_tics with unscaled elapsed microseconds.
 * However, entities use their timeline to determine how much time has
 * actually passed for them. real_delta_tics should therefore not be
 * used to update entity state directly, as it does not account for entity
 * relative time scaling or simulation slowdown.
 *
 * FIELDS:
 * - world: The engine owned registry containing the client's entities and
 * components.
 * - network: The engine owned network client providing this frame's server
 * messages and queuing messages to send to the server.
 * - input: The input state for the current frame.
 * - real_delta_tics: Unscaled elapsed time since the previous frame, in
 * microseconds.
 * - output_width: The current width of the rendering output.
 * - output_height: The current height of the rendering output.
 * - audio: The engine owned audio manager used to load and play sounds and
 * music.
 * - camera: The engine owned camera used to convert between screen and world
 * coordinates.
 */
struct ClientFrameContext {
    Registry &world;
    NetworkClient &network;
    InputManager &input;
    TicCount real_delta_tics;
    std::int32_t output_width;
    std::int32_t output_height;
    AudioManager &audio;
    Camera2D &camera;
};

/**
 * Any client that is to be run by the engine must implement this interface.
 * The engine manages the window, input, networking, and rendering while the
 * client implements the game's view and controls.
 */
class IClient {
public:
    virtual ~IClient() = default;

    /**
     * Initializes the client once at the beginning of its lifecycle.
     *
     * @param context The context providing application services and the
     * network client used to establish the server connection.
     */
    virtual void Initialize(ClientContext &context) = 0;

    /**
     * Builds the game specific portion of a join request.
     * Called once when the engine handles a new Connect request.
     *
     * @return The game's join request bytes.
     */
    virtual NetworkMessage CreateJoinRequest() = 0;

    /**
     * Handles acceptance, rejection, or timeout of the join request.
     * Called once by the engine before the frame's Update. The connection
     * state has already been updated, so accepted clients may queue messages.
     *
     * @param result The outcome and the game's response bytes.
     */
    virtual void HandleJoinResult(const JoinResult &result) = 0;

    /**
     * Handles the loss of an established server connection due to timeout.
     * Called once on the main thread before Update, after the engine has
     * marked the client as disconnected. A pending join's timeout is reported
     * through HandleJoinResult instead.
     */
    virtual void HandleDisconnect() {}

    /**
     * Updates the client's view and controls once per frame, after local input
     * and server messages have been collected and before rendering.
     * Messages queued here are sent to the server during this frame.
     *
     * @param context The context for the current client frame.
     */
    virtual void Update(const ClientFrameContext &context) = 0;

    /**
     * Determines whether the client should quit its frame loop.
     *
     * @return True if the client should quit, false otherwise.
     */
    virtual bool ShouldQuit() const { return false; }

    /**
     * Determines the interval between frames in tics.
     * The engine sleeps for the remaining time until the next frame, if any.
     *
     * @return The frame interval in microseconds. Return 0 to disable pacing.
     */
    virtual TicCount GetFrameIntervalTics() const { return 0; }
};

} // namespace svanes
