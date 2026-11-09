#include <svanes/input/input_events.hpp>

namespace svanes {

KeyInputEvent::KeyInputEvent(Key key, bool down) : key(key), down(down) {}

MouseButtonInputEvent::MouseButtonInputEvent(MouseButton button, bool down)
    : button(button), down(down) {}

MouseMotionInputEvent::MouseMotionInputEvent(Vector2D position, Vector2D delta)
    : position(position), delta(delta) {}

} // namespace svanes
