#include <svanes/input.hpp>

namespace svanes {

void InputManager::BeginFrame()
{
    previous_ = current_;
    mouse_previous_ = mouse_current_;
    mouse_delta_this_frame_ = {};
    mouse_wheel_this_frame_ = {};
    text_input_this_frame_.clear();
}

void InputManager::HandleEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        current_[event.key.scancode] = true;
        break;
    case SDL_EVENT_KEY_UP:
        current_[event.key.scancode] = false;
        break;
    case SDL_EVENT_TEXT_INPUT:
        text_input_this_frame_ += event.text.text;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        mouse_current_[event.button.button] = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        mouse_current_[event.button.button] = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        mouse_position_ = {event.motion.x, event.motion.y};
        mouse_delta_this_frame_.x += event.motion.xrel;
        mouse_delta_this_frame_.y += event.motion.yrel;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        mouse_wheel_this_frame_.x += event.wheel.x;
        mouse_wheel_this_frame_.y += event.wheel.y;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        current_.fill(false);
        mouse_current_.fill(false);
        break;
    default:
        break;
    }
}

bool InputManager::IsDown(SDL_Scancode scancode) const
{
    return current_[scancode];
}

bool InputManager::WasPressed(SDL_Scancode scancode) const
{
    return current_[scancode] && !previous_[scancode];
}

bool InputManager::WasReleased(SDL_Scancode scancode) const
{
    return !current_[scancode] && previous_[scancode];
}

bool InputManager::IsMouseButtonDown(Uint8 button) const
{
    return mouse_current_[button];
}

bool InputManager::WasMouseButtonPressed(Uint8 button) const
{
    return mouse_current_[button] && !mouse_previous_[button];
}

bool InputManager::WasMouseButtonReleased(Uint8 button) const
{
    return !mouse_current_[button] && mouse_previous_[button];
}

SDL_FPoint InputManager::MousePosition() const
{
    return mouse_position_;
}

SDL_FPoint InputManager::MouseDeltaThisFrame() const
{
    return mouse_delta_this_frame_;
}

SDL_FPoint InputManager::MouseWheelThisFrame() const
{
    return mouse_wheel_this_frame_;
}

void InputManager::StartTextInput(SDL_Window* window)
{
    SDL_StartTextInput(window);
    text_input_active_ = true;
}

void InputManager::StopTextInput(SDL_Window* window)
{
    SDL_StopTextInput(window);
    text_input_active_ = false;
}

bool InputManager::IsTextInputActive() const
{
    return text_input_active_;
}

const std::string& InputManager::TextInputThisFrame() const
{
    return text_input_this_frame_;
}

} // namespace svanes
