#pragma once

#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>

namespace svanes {

class AudioManager;
class Camera2D;
class InputManager;
class Registry;
class TextureManager;

/**
 * Defines relevant context for a game.
 * For games to have their textures managed by the engine,
 * they must use the TextureManager provided in this context.
 *
 * FIELDS:
 * - world: The engine owned registry containing the game's entities and
 * components.
 * - assets: The engine owned texture manager used to load and create textures.
 * - audio: The engine owned audio manager used to load and play sounds and
 * music.
 * - camera: The engine owned camera used to convert between screen and world
 * coordinates.
 * - output_width: The current width of the rendering output.
 * - output_height: The current height of the rendering output.
 * - gravity: The engine owned gravity vector applied to entities with
 * Kinematic2D and Gravity components, in world units per local tic squared.
 * - concurrency: The number of worker threads to use for parallel execution.
 * Defaults to 1, which means no parallel execution.
 */
struct GameContext {
    Registry &world;
    TextureManager &assets;
    AudioManager &audio;
    Camera2D &camera;
    std::int32_t output_width;
    std::int32_t output_height;
    Vector2D &gravity;
    std::uint32_t concurrency = 1;
};

/**
 * Defines relevant context for a single frame of a game.
 * For games to have their input managed by the engine,
 * they must use the InputManager provided in this context.
 * The engine fills real_delta_tics with unscaled elapsed microseconds.
 * However, entities use their timeline to determine how much time has
 * actually passed for them. real_delta_tics should therefore not be
 * used to update entity state directly, as it does not account for entity
 * relative time scaling.
 *
 * FIELDS:
 * - world: The engine owned registry containing the game's entities and
 * components.
 * - input: The input state for the current frame.
 * - real_delta_tics: Unscaled elapsed time since the previous frame, in
 * microseconds.
 * - output_width: The current width of the rendering output.
 * - output_height: The current height of the rendering output.
 * - audio: The engine owned audio manager used to load and play sounds and
 * music.
 * - camera: The engine owned camera used to convert between screen and world
 * coordinates.
 * - gravity: The engine owned gravity vector applied to entities with
 * Kinematic2D and Gravity components, in world units per local tic squared.
 */
struct FrameContext {
    Registry &world;
    InputManager &input;
    TicCount real_delta_tics;
    std::int32_t output_width;
    std::int32_t output_height;
    AudioManager &audio;
    Camera2D &camera;
    Vector2D &gravity;
};

/**
 * Any game that is to be run by the engine must implement this interface.
 * It provides a bridge between the engine and the game, allowing the engine
 * to manage the game loop and rendering while the game implements its own
 * logic.
 */
class IGame {
public:
    virtual ~IGame() = default;

    /**
     * Initializes the game with the provided context.
     * This method is called once at the start of the game.
     * @param context The context for the game, providing access to the
     * TextureManager.
     */
    virtual void Initialize(GameContext &context) = 0;

    /**
     * Updates the game state based on the provided frame context.
     * This method is called once per frame, allowing the game to process input
     * and update its state.
     * @param frame The context for the current frame, providing access to the
     * InputManager and the time elapsed since the last frame.
     */
    virtual void Update(const FrameContext &frame) = 0;

    /**
     * Determines whether the game should quit on the next
     * iteration of the game loop.
     * @return True if the game should quit, false otherwise.
     */
    virtual bool ShouldQuit() const { return false; }
};

} // namespace svanes
