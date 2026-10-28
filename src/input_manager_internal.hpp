#pragma once

#include <svanes/input.hpp>

#include <SDL3/SDL.h>

#include <optional>

namespace svanes::internal {

/**
 * Internal class for managing input state.
 * This class is responsible for handling the internal logic of input
 * management, including polling SDL for keyboard and mouse state.
 * This class should not be used directly by the user of the engine.
 */
class InputManagerInternal final {
public:
    /**
     * Prepares the InputManager for a new frame.
     * Saves the current key and mouse button states as the previous states,
     * so the manager can detect presses and releases after polling. Resets
     * this frame's mouse movement to (0, 0).
     *
     * @param input The InputManager instance to prepare for the new frame.
     */
    static void BeginFrame(InputManager &input);

    /**
     * Reads the keyboard and mouse state from SDL and updates the manager's
     * current input state. Keys and buttons are released while the window
     * lacks keyboard focus. Call once per frame after BeginFrame, on the
     * main thread.
     *
     * @param input The InputManager instance to update.
     * @param window The window receiving input.
     */
    static void Poll(InputManager &input, SDL_Window *window);

private:
    /**
     * Translates an SDL_Scancode to the engine's Key enum.
     * @param scancode The SDL_Scancode to translate.
     * @return An optional Key value. If the scancode does not correspond to a
     * known key, std::nullopt is returned.
     */
    static std::optional<Key> TranslateKey(SDL_Scancode scancode);

    /**
     * Translates an SDL mouse button code to the engine's MouseButton enum.
     * @param button The SDL mouse button code to translate.
     * @return An optional MouseButton value.
     *   If the button code does not correspond to a known mouse button,
     * std::nullopt is returned.
     */
    static std::optional<MouseButton> TranslateMouseButton(std::uint8_t button);
};

} // namespace svanes::internal
