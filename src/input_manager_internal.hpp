#pragma once

#include <svanes/input.hpp>

#include <SDL3/SDL.h>

#include <optional>

namespace svanes::internal {

/** 
 * Internal class for managing input state.
 * This class is responsible for handling the internal logic of input management,
 * including translating SDL events to the engine's input system.
 * This class should not be used directly by the user of the engine.
 */
class InputManagerInternal final {
public:

    /**
     * Prepares the InputManager for a new frame.
     * This should be called at the start of each frame to update the previous state
     * and reset the current state for keys and mouse buttons.
     * 
     * In total, this function must:
     * 
     * - Update the previous state of keys and mouse buttons to match the current state.
     * 
     * - Reset the current state of keys and mouse buttons to false (not pressed).
     * 
     * - Reset the mouse delta and wheel movement for this frame to (0, 0).
     * 
     * - Clear the text input received during the previous frame.
     * 
     * 
     * In general, this function should be kept up to date with any changes 
     * to the InputManager's state that need to be reset or updated at the start of each frame.
     * 
     * @param input The InputManager instance to prepare for the new frame.
     */
    static void BeginFrame(InputManager& input);

    /**
     * Handles an SDL_Event and updates the InputManager state accordingly.
     * This function translates SDL events to the engine's input system.
     * @param input The InputManager instance to update.
     * @param event The SDL_Event to handle.
     */
    static void HandleEvent(InputManager& input, const SDL_Event& event);

    /**
     * Synchronizes the text input state with SDL's text input handling.
     * This function checks if text input has been requested or stopped and updates
     * the InputManager's state accordingly.
     * @param input The InputManager instance to synchronize.
     * @param window The SDL_Window used for text input.
     */
    static void SynchronizeTextInput(InputManager& input, SDL_Window* window);

private:

    /**
     * Translates an SDL_Scancode to the engine's Key enum.
     * @param scancode The SDL_Scancode to translate.
     * @return An optional Key value. If the scancode does not correspond to a known key, 
     *   std::nullopt is returned.
     */
    static std::optional<Key> TranslateKey(SDL_Scancode scancode);

    /**
     * Translates an SDL mouse button code to the engine's MouseButton enum.
     * @param button The SDL mouse button code to translate.
     * @return An optional MouseButton value. 
     *   If the button code does not correspond to a known mouse button, std::nullopt is returned.
     */
    static std::optional<MouseButton> TranslateMouseButton(std::uint8_t button);
};

}
