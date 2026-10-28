#include "input_manager_internal.hpp"

namespace svanes {

void internal::InputManagerInternal::BeginFrame(InputManager &input) {
    input.previous = input.current;
    input.mouse_previous = input.mouse_current;
    input.mouse_delta_this_frame = {};
}

void internal::InputManagerInternal::Poll(InputManager &input,
                                          SDL_Window *window) {
    SDL_PumpEvents();

    // Start with all keys and buttons released. If the window is unfocused,
    // they stay released. Otherwise, we update them with the polled state.

    input.current.fill(false);
    input.mouse_current.fill(false);

    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    const auto mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);

    // We read the accumulated movement even while unfocused, so it cannot build
    // up and appear as a jump when the window regains focus.
    float delta_x = 0.0F;
    float delta_y = 0.0F;
    SDL_GetRelativeMouseState(&delta_x, &delta_y);

    if (SDL_GetKeyboardFocus() != window) {
        return;
    }

    std::int32_t key_count = 0;
    const bool *keys = SDL_GetKeyboardState(&key_count);

    // For every scancode, we translate it to our Key enum and update the
    // current state.
    for (std::int32_t scancode = 0; scancode < key_count; ++scancode) {
        const auto key = TranslateKey(static_cast<SDL_Scancode>(scancode));
        if (key.has_value()) {
            input.current.at(static_cast<std::size_t>(*key)) = keys[scancode];
        }
    }

    // For every mouse button, we translate it to our MouseButton enum and
    // update the current state.
    for (std::uint8_t code = SDL_BUTTON_LEFT; code <= SDL_BUTTON_X2; ++code) {
        const auto button = TranslateMouseButton(code);
        if (button.has_value()) {
            input.mouse_current.at(static_cast<std::size_t>(*button)) =
                (mouse_buttons & SDL_BUTTON_MASK(code)) != 0;
        }
    }

    // If the window has focus, we update the mouse position and delta for this
    // frame.
    if (SDL_GetMouseFocus() == window) {
        input.mouse_position = {mouse_x, mouse_y};
        input.mouse_delta_this_frame = {delta_x, delta_y};
    }
}

std::optional<Key>
internal::InputManagerInternal::TranslateKey(SDL_Scancode scancode) {
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

std::optional<MouseButton>
internal::InputManagerInternal::TranslateMouseButton(std::uint8_t button) {
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
