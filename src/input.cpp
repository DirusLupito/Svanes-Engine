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
    case SDL_SCANCODE_B:
        return Key::B;
    case SDL_SCANCODE_C:
        return Key::C;
    case SDL_SCANCODE_E:
        return Key::E;
    case SDL_SCANCODE_F:
        return Key::F;
    case SDL_SCANCODE_G:
        return Key::G;
    case SDL_SCANCODE_H:
        return Key::H;
    case SDL_SCANCODE_I:
        return Key::I;
    case SDL_SCANCODE_J:
        return Key::J;
    case SDL_SCANCODE_K:
        return Key::K;
    case SDL_SCANCODE_L:
        return Key::L;
    case SDL_SCANCODE_M:
        return Key::M;
    case SDL_SCANCODE_N:
        return Key::N;
    case SDL_SCANCODE_O:
        return Key::O;
    case SDL_SCANCODE_P:
        return Key::P;
    case SDL_SCANCODE_Q:
        return Key::Q;
    case SDL_SCANCODE_R:
        return Key::R;
    case SDL_SCANCODE_T:
        return Key::T;
    case SDL_SCANCODE_U:
        return Key::U;
    case SDL_SCANCODE_V:
        return Key::V;
    case SDL_SCANCODE_X:
        return Key::X;
    case SDL_SCANCODE_Y:
        return Key::Y;
    case SDL_SCANCODE_Z:
        return Key::Z;
    case SDL_SCANCODE_0:
        return Key::Digit0;
    case SDL_SCANCODE_1:
        return Key::Digit1;
    case SDL_SCANCODE_2:
        return Key::Digit2;
    case SDL_SCANCODE_3:
        return Key::Digit3;
    case SDL_SCANCODE_4:
        return Key::Digit4;
    case SDL_SCANCODE_5:
        return Key::Digit5;
    case SDL_SCANCODE_6:
        return Key::Digit6;
    case SDL_SCANCODE_7:
        return Key::Digit7;
    case SDL_SCANCODE_8:
        return Key::Digit8;
    case SDL_SCANCODE_9:
        return Key::Digit9;
    case SDL_SCANCODE_RETURN:
        return Key::Enter;
    case SDL_SCANCODE_BACKSPACE:
        return Key::Backspace;
    case SDL_SCANCODE_TAB:
        return Key::Tab;
    case SDL_SCANCODE_MINUS:
        return Key::Minus;
    case SDL_SCANCODE_EQUALS:
        return Key::Equals;
    case SDL_SCANCODE_LEFTBRACKET:
        return Key::LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET:
        return Key::RightBracket;
    case SDL_SCANCODE_BACKSLASH:
        return Key::Backslash;
    case SDL_SCANCODE_SEMICOLON:
        return Key::Semicolon;
    case SDL_SCANCODE_APOSTROPHE:
        return Key::Apostrophe;
    case SDL_SCANCODE_GRAVE:
        return Key::Grave;
    case SDL_SCANCODE_COMMA:
        return Key::Comma;
    case SDL_SCANCODE_PERIOD:
        return Key::Period;
    case SDL_SCANCODE_SLASH:
        return Key::Slash;
    case SDL_SCANCODE_CAPSLOCK:
        return Key::CapsLock;
    case SDL_SCANCODE_F1:
        return Key::F1;
    case SDL_SCANCODE_F2:
        return Key::F2;
    case SDL_SCANCODE_F3:
        return Key::F3;
    case SDL_SCANCODE_F4:
        return Key::F4;
    case SDL_SCANCODE_F5:
        return Key::F5;
    case SDL_SCANCODE_F6:
        return Key::F6;
    case SDL_SCANCODE_F7:
        return Key::F7;
    case SDL_SCANCODE_F8:
        return Key::F8;
    case SDL_SCANCODE_F9:
        return Key::F9;
    case SDL_SCANCODE_F10:
        return Key::F10;
    case SDL_SCANCODE_F11:
        return Key::F11;
    case SDL_SCANCODE_F12:
        return Key::F12;
    case SDL_SCANCODE_F13:
        return Key::F13;
    case SDL_SCANCODE_F14:
        return Key::F14;
    case SDL_SCANCODE_F15:
        return Key::F15;
    case SDL_SCANCODE_F16:
        return Key::F16;
    case SDL_SCANCODE_F17:
        return Key::F17;
    case SDL_SCANCODE_F18:
        return Key::F18;
    case SDL_SCANCODE_F19:
        return Key::F19;
    case SDL_SCANCODE_F20:
        return Key::F20;
    case SDL_SCANCODE_F21:
        return Key::F21;
    case SDL_SCANCODE_F22:
        return Key::F22;
    case SDL_SCANCODE_F23:
        return Key::F23;
    case SDL_SCANCODE_F24:
        return Key::F24;
    case SDL_SCANCODE_PRINTSCREEN:
        return Key::PrintScreen;
    case SDL_SCANCODE_SCROLLLOCK:
        return Key::ScrollLock;
    case SDL_SCANCODE_PAUSE:
        return Key::Pause;
    case SDL_SCANCODE_INSERT:
        return Key::Insert;
    case SDL_SCANCODE_HOME:
        return Key::Home;
    case SDL_SCANCODE_PAGEUP:
        return Key::PageUp;
    case SDL_SCANCODE_DELETE:
        return Key::Delete;
    case SDL_SCANCODE_END:
        return Key::End;
    case SDL_SCANCODE_PAGEDOWN:
        return Key::PageDown;
    case SDL_SCANCODE_UP:
        return Key::Up;
    case SDL_SCANCODE_DOWN:
        return Key::Down;
    case SDL_SCANCODE_NUMLOCKCLEAR:
        return Key::NumLock;
    case SDL_SCANCODE_KP_DIVIDE:
        return Key::KeypadDivide;
    case SDL_SCANCODE_KP_MULTIPLY:
        return Key::KeypadMultiply;
    case SDL_SCANCODE_KP_MINUS:
        return Key::KeypadMinus;
    case SDL_SCANCODE_KP_PLUS:
        return Key::KeypadPlus;
    case SDL_SCANCODE_KP_ENTER:
        return Key::KeypadEnter;
    case SDL_SCANCODE_KP_0:
        return Key::Keypad0;
    case SDL_SCANCODE_KP_1:
        return Key::Keypad1;
    case SDL_SCANCODE_KP_2:
        return Key::Keypad2;
    case SDL_SCANCODE_KP_3:
        return Key::Keypad3;
    case SDL_SCANCODE_KP_4:
        return Key::Keypad4;
    case SDL_SCANCODE_KP_5:
        return Key::Keypad5;
    case SDL_SCANCODE_KP_6:
        return Key::Keypad6;
    case SDL_SCANCODE_KP_7:
        return Key::Keypad7;
    case SDL_SCANCODE_KP_8:
        return Key::Keypad8;
    case SDL_SCANCODE_KP_9:
        return Key::Keypad9;
    case SDL_SCANCODE_KP_PERIOD:
        return Key::KeypadPeriod;
    case SDL_SCANCODE_KP_EQUALS:
        return Key::KeypadEquals;
    case SDL_SCANCODE_LCTRL:
        return Key::LeftControl;
    case SDL_SCANCODE_LSHIFT:
        return Key::LeftShift;
    case SDL_SCANCODE_LALT:
        return Key::LeftAlt;
    case SDL_SCANCODE_LGUI:
        return Key::LeftSuper;
    case SDL_SCANCODE_RCTRL:
        return Key::RightControl;
    case SDL_SCANCODE_RSHIFT:
        return Key::RightShift;
    case SDL_SCANCODE_RALT:
        return Key::RightAlt;
    case SDL_SCANCODE_RGUI:
        return Key::RightSuper;
    case SDL_SCANCODE_APPLICATION:
        return Key::Menu;
    case SDL_SCANCODE_NONUSHASH:
        return Key::NonUsHash;
    case SDL_SCANCODE_NONUSBACKSLASH:
        return Key::NonUsBackslash;
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
