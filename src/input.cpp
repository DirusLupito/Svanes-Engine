#include <svanes/input.hpp>

namespace svanes {

void InputManager::BeginFrame()
{
    previous = current;
    mouse_previous = mouse_current;
    mouse_delta_this_frame = {};
    mouse_wheel_this_frame = {};
    text_input_this_frame.clear();
}

void InputManager::HandleEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        current[event.key.scancode] = true;
        break;
    case SDL_EVENT_KEY_UP:
        current[event.key.scancode] = false;
        break;
    case SDL_EVENT_TEXT_INPUT:
        text_input_this_frame += event.text.text;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        mouse_current[event.button.button] = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        mouse_current[event.button.button] = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        mouse_position = {event.motion.x, event.motion.y};
        mouse_delta_this_frame.x += event.motion.xrel;
        mouse_delta_this_frame.y += event.motion.yrel;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        mouse_wheel_this_frame.x += event.wheel.x;
        mouse_wheel_this_frame.y += event.wheel.y;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        current.fill(false);
        mouse_current.fill(false);
        break;
    default:
        break;
    }
}

bool InputManager::IsDown(SDL_Scancode scancode) const
{
    return current[scancode];
}

bool InputManager::WasPressed(SDL_Scancode scancode) const
{
    return current[scancode] && !previous[scancode];
}

bool InputManager::WasReleased(SDL_Scancode scancode) const
{
    return !current[scancode] && previous[scancode];
}

bool InputManager::IsMouseButtonDown(Uint8 button) const
{
    return mouse_current[button];
}

bool InputManager::WasMouseButtonPressed(Uint8 button) const
{
    return mouse_current[button] && !mouse_previous[button];
}

bool InputManager::WasMouseButtonReleased(Uint8 button) const
{
    return !mouse_current[button] && mouse_previous[button];
}

SDL_FPoint InputManager::MousePosition() const
{
    return mouse_position;
}

SDL_FPoint InputManager::MouseDeltaThisFrame() const
{
    return mouse_delta_this_frame;
}

SDL_FPoint InputManager::MouseWheelThisFrame() const
{
    return mouse_wheel_this_frame;
}

void InputManager::StartTextInput(SDL_Window* window)
{
    SDL_StartTextInput(window);
    text_input_active = true;
}

void InputManager::StopTextInput(SDL_Window* window)
{
    SDL_StopTextInput(window);
    text_input_active = false;
}

bool InputManager::IsTextInputActive() const
{
    return text_input_active;
}

const std::string& InputManager::TextInputThisFrame() const
{
    return text_input_this_frame;
}

} // namespace svanes
