#include <svanes/input/input_events.hpp>

namespace svanes {

KeyInputEvent::KeyInputEvent(Key key, bool down) : key(key), down(down) {}

std::string KeyInputEvent::GetStaticEventType() { return "svanes.input.key"; }

const std::string KeyInputEvent::GetEventType() const {
    return GetStaticEventType();
}

MouseButtonInputEvent::MouseButtonInputEvent(MouseButton button, bool down)
    : button(button), down(down) {}

std::string MouseButtonInputEvent::GetStaticEventType() {
    return "svanes.input.mouse_button";
}

const std::string MouseButtonInputEvent::GetEventType() const {
    return GetStaticEventType();
}

MouseMotionInputEvent::MouseMotionInputEvent(Vector2D position, Vector2D delta)
    : position(position), delta(delta) {}

std::string MouseMotionInputEvent::GetStaticEventType() {
    return "svanes.input.mouse_motion";
}

const std::string MouseMotionInputEvent::GetEventType() const {
    return GetStaticEventType();
}

} // namespace svanes
