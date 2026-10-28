#pragma once

#include <svanes/input/input_types.hpp>
#include <svanes/vector2d.hpp>

#include <array>
#include <cstddef>

namespace svanes {

class KeyInputEvent;
class MouseButtonInputEvent;
class MouseMotionInputEvent;

namespace internal {

class InputManagerInternal;

}

/**
 * Class that manages input state for keys and mouse buttons,
 * as well as mouse position and movement.
 * Mostly a wrapper around SDL's input handling.
 */
class InputManager final {
public:
    /**
     * Checks if a specific key is currently pressed down.
     * @param key The key to check.
     * @return True if the key is down, false otherwise.
     */
    bool IsDown(Key key) const;

    /**
     * Checks if a specific key was pressed in the current frame.
     * @param key The key to check.
     * @return True if the key was pressed this frame, false otherwise.
     */
    bool WasPressed(Key key) const;

    /**
     * Checks if a specific key was released in the current frame.
     * @param key The key to check.
     * @return True if the key was released this frame, false otherwise.
     */
    bool WasReleased(Key key) const;

    /**
     * Checks if a specific mouse button is currently pressed down.
     * @param button The mouse button to check.
     * @return True if the mouse button is down, false otherwise.
     */
    bool IsMouseButtonDown(MouseButton button) const;

    /**
     * Checks if a specific mouse button was pressed in the current frame.
     * @param button The mouse button to check.
     * @return True if the mouse button was pressed this frame, false otherwise.
     */
    bool WasMouseButtonPressed(MouseButton button) const;

    /**
     * Checks if a specific mouse button was released in the current frame.
     * @param button The mouse button to check.
     * @return True if the mouse button was released this frame, false
     * otherwise.
     */
    bool WasMouseButtonReleased(MouseButton button) const;


    /**
     * Gets the current mouse position.
     * Measured relative to the window, with (0, 0) being the top-left corner of
     * the window.
     * @return The current mouse position as a Vector2D.
     */
    Vector2D MousePosition() const;

    /**
     * Gets the mouse movement delta (change in position) for this frame.
     * @return The mouse delta for this frame as a Vector2D.
     */
    Vector2D MouseDeltaThisFrame() const;

private:
    /**
     * Updates the state of the key identified by an input event. Press and
     * release queries compare this state with the previous frame's state.
     *
     * @param event The key and whether it is now held down.
     * @throws std::out_of_range if event.key is not a valid key.
     */
    void HandleKeyInput(const KeyInputEvent &event);

    /**
     * Updates the state of the mouse button identified by an input event.
     *
     * @param event The mouse button and whether it is now held down.
     * @throws std::out_of_range if event.button is not a valid mouse button.
     */
    void HandleMouseButtonInput(const MouseButtonInputEvent &event);

    /**
     * Updates the cursor position and adds the event's movement to this
     * frame's mouse delta. Multiple movement events in a frame contribute
     * to the same accumulated delta.
     *
     * @param event The cursor position and movement to apply.
     */
    void HandleMouseMotionInput(const MouseMotionInputEvent &event);

    // Constants for the number of keys and mouse buttons, used to size the
    // arrays.

    static constexpr std::size_t kKeyCount =
        static_cast<std::size_t>(Key::Count);

    static constexpr std::size_t kMouseButtonCount =
        static_cast<std::size_t>(MouseButton::Count);

    // The previous array will be 1 frame behind the current array.
    // For both keys and mouse buttons.

    // Tracks the current state of keys. This array is indexed by the Key enum
    // values. true means the key is currently down, false means it is up.
    std::array<bool, kKeyCount> current{};

    // Tracks the previous state of keys. This array is indexed by the Key enum
    // values. true means the key was down in the previous frame, false means it
    // was up.
    std::array<bool, kKeyCount> previous{};

    // Tracks the current state of mouse buttons. This array is indexed by the
    // MouseButton enum values. true means the mouse button is currently down,
    // false means it is up.
    std::array<bool, kMouseButtonCount> mouse_current{};

    // Tracks the previous state of mouse buttons. This array is indexed by the
    // MouseButton enum values. true means the mouse button was down in the
    // previous frame, false means it was up
    std::array<bool, kMouseButtonCount> mouse_previous{};

    // Current mouse position relative to the window, with (0, 0) being the
    // top-left corner of the window.
    Vector2D mouse_position{};

    // Mouse movement delta (change in position) for this frame.
    // This should be reset to (0, 0) at the start of each frame.
    Vector2D mouse_delta_this_frame{};

    // Expose the private members to the internal InputManagerInternal class for
    // managing input state.
    friend class internal::InputManagerInternal;
};

} // namespace svanes
