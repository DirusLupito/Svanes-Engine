#include <svanes/input.hpp>

#include "input_manager_internal.hpp"

#include <stdexcept>
#include <string>

namespace svanes {

// To test if something is down, we can just look at the current state.
// If it's down, it's down :)

// To test if something was just pressed, we can add in the previous state.
// If it's down now, but was not down in the previous frame, then it was just pressed.

// Symmetrically, to test if something was just released, we can see if
// it's up now, but was down in the previous frame. If so, it was just released.

bool InputManager::IsDown(Key key) const
{
    return current.at(static_cast<std::size_t>(key));
}

bool InputManager::WasPressed(Key key) const
{
    const std::size_t index = static_cast<std::size_t>(key);
    return current.at(index) && !previous.at(index);
}

bool InputManager::WasReleased(Key key) const
{
    const std::size_t index = static_cast<std::size_t>(key);
    return !current.at(index) && previous.at(index);
}

bool InputManager::IsMouseButtonDown(MouseButton button) const
{
    return mouse_current.at(static_cast<std::size_t>(button));
}

bool InputManager::WasMouseButtonPressed(MouseButton button) const
{
    const std::size_t index = static_cast<std::size_t>(button);
    return mouse_current.at(index) && !mouse_previous.at(index);
}

bool InputManager::WasMouseButtonReleased(MouseButton button) const
{
    const std::size_t index = static_cast<std::size_t>(button);
    return !mouse_current.at(index) && mouse_previous.at(index);
}

// POJO slop... but in C++ 

InputVector InputManager::MousePosition() const
{
    return mouse_position;
}

InputVector InputManager::MouseDeltaThisFrame() const
{
    return mouse_delta_this_frame;
}

InputVector InputManager::MouseWheelThisFrame() const
{
    return mouse_wheel_this_frame;
}

void InputManager::StartTextInput()
{
    text_input_requested = true;
}

void InputManager::StopTextInput()
{
    text_input_requested = false;
}

bool InputManager::IsTextInputActive() const
{
    return text_input_active;
}

const std::string& InputManager::TextInputThisFrame() const
{
    return text_input_this_frame;
}

void internal::InputManagerInternal::BeginFrame(InputManager& input)
{
    input.previous = input.current;
    input.mouse_previous = input.mouse_current;
    input.mouse_delta_this_frame = {};
    input.mouse_wheel_this_frame = {};
    input.text_input_this_frame.clear();
}

void internal::InputManagerInternal::HandleEvent(InputManager& input, const SDL_Event& event)
{
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        const std::optional<Key> key = TranslateKey(event.key.scancode);
        if (key.has_value()) {
            input.current.at(static_cast<std::size_t>(*key)) = event.type == SDL_EVENT_KEY_DOWN;
        }
        break;
    }
    case SDL_EVENT_TEXT_INPUT:
        input.text_input_this_frame += event.text.text;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        const std::optional<MouseButton> button = TranslateMouseButton(event.button.button);
        if (button.has_value()) {
            input.mouse_current.at(static_cast<std::size_t>(*button)) = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        }
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
        input.mouse_position = {event.motion.x, event.motion.y};
        input.mouse_delta_this_frame.x += event.motion.xrel;
        input.mouse_delta_this_frame.y += event.motion.yrel;
        break;
    // SDL_EVENT_MOUSE_WHEEL supports two axes of scrolling with a mouse wheel.
    // But typically mice typically only produce vertical scrolling, 
    // since typical mice only have a single wheel that either scrolls up or down.
    // This would correspond to the y-axis of scrolling. 
    case SDL_EVENT_MOUSE_WHEEL:
        input.mouse_wheel_this_frame.x += event.wheel.x;
        input.mouse_wheel_this_frame.y += event.wheel.y;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        input.current.fill(false);
        input.mouse_current.fill(false);
        break;
    default:
        break;
    }
}

void internal::InputManagerInternal::SynchronizeTextInput(InputManager& input, SDL_Window* window)
{
    if (input.text_input_requested == input.text_input_active) {
        return;
    }

    const bool succeeded = input.text_input_requested
        ? SDL_StartTextInput(window)
        : SDL_StopTextInput(window);
    if (!succeeded) {
        throw std::runtime_error("Could not change text input state: " + std::string{SDL_GetError()});
    }

    input.text_input_active = input.text_input_requested;
}

std::optional<Key> internal::InputManagerInternal::TranslateKey(SDL_Scancode scancode)
{
    switch (scancode) {
    case SDL_SCANCODE_ESCAPE:
        return Key::Escape;
    case SDL_SCANCODE_LEFT:
        return Key::Left;
    case SDL_SCANCODE_RIGHT:
        return Key::Right;
    case SDL_SCANCODE_SPACE:
        return Key::Space;
    case SDL_SCANCODE_W:
        return Key::W;
    case SDL_SCANCODE_A:
        return Key::A;
    case SDL_SCANCODE_S:
        return Key::S;
    case SDL_SCANCODE_D:
        return Key::D;
    default:
        return std::nullopt;
    }
}

std::optional<MouseButton> internal::InputManagerInternal::TranslateMouseButton(std::uint8_t button)
{
    switch (button) {
    case SDL_BUTTON_LEFT:
        return MouseButton::Left;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::Middle;
    case SDL_BUTTON_RIGHT:
        return MouseButton::Right;
    case SDL_BUTTON_X1:
        return MouseButton::Mouse4;
    case SDL_BUTTON_X2:
        return MouseButton::Mouse5;
    default:
        return std::nullopt;
    }
}

} // namespace svanes
