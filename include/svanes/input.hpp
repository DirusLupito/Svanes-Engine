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
 * - Space: The Spacebar key.
 * - A: The A key.
 * - B: The B key.
 * - C: The C key.
 * - D: The D key.
 * - E: The E key.
 * - F: The F key.
 * - G: The G key.
 * - H: The H key.
 * - I: The I key.
 * - J: The J key.
 * - K: The K key.
 * - L: The L key.
 * - M: The M key.
 * - N: The N key.
 * - O: The O key.
 * - P: The P key.
 * - Q: The Q key.
 * - R: The R key.
 * - S: The S key.
 * - T: The T key.
 * - U: The U key.
 * - V: The V key.
 * - W: The W key.
 * - X: The X key.
 * - Y: The Y key.
 * - Z: The Z key.
 * 
 * - Digit0: The 0 key on the main number row.
 * - Digit1: The 1 key on the main number row.
 * - Digit2: The 2 key on the main number row.
 * - Digit3: The 3 key on the main number row.
 * - Digit4: The 4 key on the main number row.
 * - Digit5: The 5 key on the main number row.
 * - Digit6: The 6 key on the main number row.
 * - Digit7: The 7 key on the main number row.
 * - Digit8: The 8 key on the main number row.
 * - Digit9: The 9 key on the main number row.
 * 
 * - Enter: The Enter key.
 * - Backspace: The Backspace key.
 * - Tab: The Tab key.
 * - Minus: The Minus key.
 * - Equals: The Equals key.
 * - LeftBracket: The Left Bracket key.
 * - RightBracket: The Right Bracket key.
 * - Backslash: The Backslash key.
 * - Semicolon: The Semicolon key.
 * - Apostrophe: The Apostrophe key.
 * - Grave: The grave accent/backtick key.
 * - Comma: The Comma key.
 * - Period: The Period key.
 * - Slash: The Slash key.
 * - CapsLock: The Caps Lock key.
 * 
 * - F1: The F1 key.
 * - F2: The F2 key.
 * - F3: The F3 key.
 * - F4: The F4 key.
 * - F5: The F5 key.
 * - F6: The F6 key.
 * - F7: The F7 key.
 * - F8: The F8 key.
 * - F9: The F9 key.
 * - F10: The F10 key.
 * - F11: The F11 key.
 * - F12: The F12 key.
 * - F13: The F13 key.
 * - F14: The F14 key.
 * - F15: The F15 key.
 * - F16: The F16 key.
 * - F17: The F17 key.
 * - F18: The F18 key.
 * - F19: The F19 key.
 * - F20: The F20 key.
 * - F21: The F21 key.
 * - F22: The F22 key.
 * - F23: The F23 key.
 * - F24: The F24 key.
 * 
 * - PrintScreen: The Print Screen key.
 * - ScrollLock: The Scroll Lock key.
 * - Pause: The Pause key.
 * - Insert: The Insert key.
 * - Home: The Home key.
 * - PageUp: The Page Up key.
 * - Delete: The Delete key.
 * - End: The End key.
 * - PageDown: The Page Down key.
 * 
 * - Up: The Up Arrow key.
 * - Down: The Down Arrow key.
 * - Left: The Left Arrow key.
 * - Right: The Right Arrow key.
 * 
 * - NumLock: The Num Lock key (Clear on Mac keyboards).
 * - KeypadDivide: The numeric keypad divide key.
 * - KeypadMultiply: The numeric keypad multiply key.
 * - KeypadMinus: The numeric keypad minus key.
 * - KeypadPlus: The numeric keypad plus key.
 * - KeypadEnter: The numeric keypad enter key.
 * - Keypad0: The numeric keypad 0 key.
 * - Keypad1: The numeric keypad 1 key.
 * - Keypad2: The numeric keypad 2 key.
 * - Keypad3: The numeric keypad 3 key.
 * - Keypad4: The numeric keypad 4 key.
 * - Keypad5: The numeric keypad 5 key.
 * - Keypad6: The numeric keypad 6 key.
 * - Keypad7: The numeric keypad 7 key.
 * - Keypad8: The numeric keypad 8 key.
 * - Keypad9: The numeric keypad 9 key.
 * - KeypadPeriod: The numeric keypad decimal point key.
 * - KeypadEquals: The numeric keypad equals key.
 * 
 * - LeftControl: The Left Control key.
 * - LeftShift: The Left Shift key.
 * - LeftAlt: The left Alt key (Option on Mac keyboards).
 * - LeftSuper: The left Windows, Command, or Super key.
 * - RightControl: The Right Control key.
 * - RightShift: The Right Shift key.
 * - RightAlt: The right Alt key (AltGr or Option, depending on the keyboard).
 * - RightSuper: The right Windows, Command, or Super key.
 * - Menu: The context menu key.
 * - NonUsHash: The non-US hash key used by some ISO keyboards.
 * - NonUsBackslash: The additional ISO key between Left Shift and Z.
 * 
 * - Count: A fake key used to get the number of keys in the enum. 
 *   Used to size the arrays in InputManager.
 */
enum class Key : std::uint8_t {
    Escape,
    Space,

    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,

    Enter,
    Backspace,
    Tab,
    Minus,
    Equals,
    LeftBracket,
    RightBracket,
    Backslash,
    Semicolon,
    Apostrophe,
    Grave,
    Comma,
    Period,
    Slash,
    CapsLock,

    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,

    PrintScreen,
    ScrollLock,
    Pause,
    Insert,
    Home,
    PageUp,
    Delete,
    End,
    PageDown,

    Up,
    Down,
    Left,
    Right,

    NumLock,
    KeypadDivide,
    KeypadMultiply,
    KeypadMinus,
    KeypadPlus,
    KeypadEnter,
    Keypad0,
    Keypad1,
    Keypad2,
    Keypad3,
    Keypad4,
    Keypad5,
    Keypad6,
    Keypad7,
    Keypad8,
    Keypad9,
    KeypadPeriod,
    KeypadEquals,

    LeftControl,
    LeftShift,
    LeftAlt,
    LeftSuper,
    RightControl,
    RightShift,
    RightAlt,
    RightSuper,
    Menu,

    NonUsHash,
    NonUsBackslash,
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
    Right,  // aka mouse 2
    Middle, // aka mouse 3
    Mouse4,
    Mouse5,
    Count,
};

/**
 * Struct representing a 2D vector for input purposes, 
 * such as mouse position, delta, and wheel movement.
 *
 * FIELDS:
 * - x: The horizontal component of the input vector.
 * - y: The vertical component of the input vector.
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
