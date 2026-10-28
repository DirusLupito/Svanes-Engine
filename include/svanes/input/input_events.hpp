#pragma once

#include <svanes/events/event.hpp>
#include <svanes/input/input_types.hpp>
#include <svanes/vector2d.hpp>

#include <string>

namespace svanes {

/**
 * Represents a key press or release. It identifies the key and whether it is
 * now held down.
 */
class KeyInputEvent final : public Event {
public:
    /**
     * Constructs an event describing a change in a key's state.
     *
     * @param key The key whose state changed.
     * @param down True if the key was pressed, false if it was released.
     */
    KeyInputEvent(Key key, bool down);

    /**
     * Returns the identifier used to subscribe to key input events.
     *
     * @return The event type identifier shared by all KeyInputEvent instances.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event so the event manager can find its handlers.
     *
     * @return The same identifier returned by GetStaticEventType().
     */
    const std::string GetEventType() const override;

    // The key whose state changed.
    Key key;

    // True if the key is now held down, false if it is released.
    bool down;
};

/**
 * Represents a mouse button press or release. It identifies the button and
 * whether it is now held down.
 */
class MouseButtonInputEvent final : public Event {
public:
    /**
     * Constructs an event describing a change in a mouse button's state.
     *
     * @param button The mouse button whose state changed.
     * @param down True if the button was pressed, false if it was released.
     */
    MouseButtonInputEvent(MouseButton button, bool down);

    /**
     * Returns the identifier used to subscribe to mouse button input events.
     *
     * @return The event type identifier shared by all MouseButtonInputEvent
     * instances.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event so the event manager can find its handlers.
     *
     * @return The same identifier returned by GetStaticEventType().
     */
    const std::string GetEventType() const override;

    // The mouse button whose state changed.
    MouseButton button;

    // True if the button is now held down, false if it is released.
    bool down;
};

/**
 * Represents mouse movement, including the cursor's position in the window
 * and the movement accumulated since the previous input sample.
 */
class MouseMotionInputEvent final : public Event {
public:
    /**
     * Constructs an event describing the mouse's position and movement.
     *
     * @param position The cursor position relative to the window's top-left
     * corner.
     * @param delta The mouse movement since the previous input sample.
     */
    MouseMotionInputEvent(Vector2D position, Vector2D delta);

    /**
     * Returns the identifier used to subscribe to mouse movement events.
     *
     * @return The event type identifier shared by all MouseMotionInputEvent
     * instances.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event so the event manager can find its handlers.
     *
     * @return The same identifier returned by GetStaticEventType().
     */
    const std::string GetEventType() const override;

    // Cursor position relative to the window's top-left corner.
    Vector2D position;

    // Mouse movement accumulated since the previous input sample.
    Vector2D delta;
};

} // namespace svanes
