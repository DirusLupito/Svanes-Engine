#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace svanes {

namespace internal {

class InputManagerInternal;

}

/**
 * Enumeration of keys that can be queried from the InputManager.
 * 
 * MEMBERS:
 * - Escape: The Escape key.
 * - Left: The Left Arrow key.
 * - Right: The Right Arrow key.
 * - Space: The Spacebar key.
 * - Count: A fake key used to get the number of keys in the enum. 
 *   Used to size the arrays in InputManager.
 */
enum class Key : std::uint8_t {
    Escape,
    Left,
    Right,
    Space,
    Count,
};

/**
 * Enumeration of mouse buttons that can be queried from the InputManager.
 * 
 * MEMBERS:
 * - Left: The left mouse button.
 * - Middle: The middle mouse button.
 * - Right: The right mouse button.
 * - Mouse4: The fourth mouse button (typically the back side button).
 * - Mouse5: The fifth mouse button (typically the forward side button).
 * - Count: A fake button used to get the number of buttons in the enum. 
 *   Used to size the arrays in InputManager.
 */
enum class MouseButton : std::uint8_t {
    Left,   // aka mouse 1
    Middle, // aka mouse 2
    Right,  // aka mouse 3
    Mouse4,
    Mouse5,
    Count,
};

/**
 * Struct representing a 2D vector for input purposes, 
 * such as mouse position, delta, and wheel movement.
 */
struct InputVector {
    float x = 0.0F;
    float y = 0.0F;
};

/**
 * Class that manages input state for keys and mouse buttons,
 * as well as mouse position, delta, and wheel movement.
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
     * @return True if the mouse button was released this frame, false otherwise.
     */
    bool WasMouseButtonReleased(MouseButton button) const;


    /**
     * Gets the current mouse position.
     * Measured relative to the window, with (0, 0) being the top-left corner of the window.
     * @return The current mouse position as an InputVector.
     */
    InputVector MousePosition() const;

    /**
     * Gets the mouse movement delta (change in position) for this frame.
     * @return The mouse delta for this frame as an InputVector.
     */
    InputVector MouseDeltaThisFrame() const;

    /**
     * Gets the mouse wheel movement for this frame.
     * @return The mouse wheel movement for this frame as an InputVector.
     */
    InputVector MouseWheelThisFrame() const;

    /**
     * Requests to start text input mode.
     * This will enable SDL's text input handling, allowing for text input events.
     */
    void StartTextInput();

    /**
     * Requests to stop text input mode.
     * This will disable SDL's text input handling.
     */
    void StopTextInput();

    /**
     * Checks if text input mode is currently active.
     * @return True if text input is active, false otherwise.
     */
    bool IsTextInputActive() const;

    /**
     * Gets the text input received during this frame.
     * @return A string containing the text input for this frame.
     */
    const std::string& TextInputThisFrame() const;

private:

    // Constants for the number of keys and mouse buttons, used to size the arrays.

    static constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);
    static constexpr std::size_t kMouseButtonCount = static_cast<std::size_t>(MouseButton::Count);

    // The previous array will be 1 frame behind the current array. 
    // For both keys and mouse buttons.

    // Tracks the current state of keys. This array is indexed by the Key enum values.
    // true means the key is currently down, false means it is up.
    std::array<bool, kKeyCount> current{};

    // Tracks the previous state of keys. This array is indexed by the Key enum values.
    // true means the key was down in the previous frame, false means it was up.
    std::array<bool, kKeyCount> previous{};

    // Tracks the current state of mouse buttons. This array is indexed by the MouseButton enum values.
    // true means the mouse button is currently down, false means it is up.
    std::array<bool, kMouseButtonCount> mouse_current{};

    // Tracks the previous state of mouse buttons. This array is indexed by the MouseButton enum values.
    // true means the mouse button was down in the previous frame, false means it was up
    std::array<bool, kMouseButtonCount> mouse_previous{};

    // Current mouse position relative to the window, with (0, 0) being the top-left corner of the window.
    InputVector mouse_position{};

    // Mouse movement delta (change in position) for this frame.
    // This should be reset to (0, 0) at the start of each frame.
    InputVector mouse_delta_this_frame{};

    // Mouse wheel movement for this frame.
    // This should be reset to (0, 0) at the start of each frame.
    InputVector mouse_wheel_this_frame{};

    // Tracks whether text input has been requested for this frame.
    bool text_input_requested = false;

    // Tracks whether text input is currently active.
    bool text_input_active = false;

    // Stores the text input received during this frame.
    // This should be cleared at the start of each frame.
    std::string text_input_this_frame;


    // Expose the private members to the internal InputManagerInternal class for managing input state.
    friend class internal::InputManagerInternal;
};

} // namespace svanes
