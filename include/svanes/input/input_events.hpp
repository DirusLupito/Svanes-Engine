#pragma once

#include <svanes/events/event.hpp>
#include <svanes/input/input_types.hpp>
#include <svanes/vector2d.hpp>

#include <string_view>

namespace svanes {

/**
 * Represents a key press or release. It identifies the key and whether it is
 * now held down.
 */
class KeyInputEvent final : public EventOf<KeyInputEvent> {
public:
    // The unique name identifying key press and release events.
    static constexpr std::string_view EventName = "svanes.input.key";

    /**
     * Constructs an event describing a change in a key's state.
     *
     * @param key The key whose state changed.
     * @param down True if the key was pressed, false if it was released.
     */
    KeyInputEvent(Key key, bool down);

    // The key whose state changed.
    Key key;

    // True if the key is now held down, false if it is released.
    bool down;
};

/**
 * Represents a mouse button press or release. It identifies the button and
 * whether it is now held down.
 */
class MouseButtonInputEvent final : public EventOf<MouseButtonInputEvent> {
public:
    // The unique name identifying mouse button press and release events.
    static constexpr std::string_view EventName = "svanes.input.mouse_button";

    /**
     * Constructs an event describing a change in a mouse button's state.
     *
     * @param button The mouse button whose state changed.
     * @param down True if the button was pressed, false if it was released.
     */
    MouseButtonInputEvent(MouseButton button, bool down);

    // The mouse button whose state changed.
    MouseButton button;

    // True if the button is now held down, false if it is released.
    bool down;
};

/**
 * Represents mouse movement, including the cursor's position in the window
 * and the movement accumulated since the previous input sample.
 */
class MouseMotionInputEvent final : public EventOf<MouseMotionInputEvent> {
public:
    // The unique name identifying mouse movement events.
    static constexpr std::string_view EventName = "svanes.input.mouse_motion";

    /**
     * Constructs an event describing the mouse's position and movement.
     *
     * @param position The cursor position relative to the window's top-left
     * corner.
     * @param delta The mouse movement since the previous input sample.
     */
    MouseMotionInputEvent(Vector2D position, Vector2D delta);

    // Cursor position relative to the window's top-left corner.
    Vector2D position;

    // Mouse movement accumulated since the previous input sample.
    Vector2D delta;
};

} // namespace svanes
