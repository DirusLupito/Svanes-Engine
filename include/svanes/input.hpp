#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <string>

namespace svanes {

class InputManager final {
public:
    void BeginFrame();
    void HandleEvent(const SDL_Event& event);

bool IsDown(SDL_Scancode scancode) const;
bool WasPressed(SDL_Scancode scancode) const;
bool WasReleased(SDL_Scancode scancode) const;

bool IsMouseButtonDown(Uint8 button) const;
bool WasMouseButtonPressed(Uint8 button) const;
bool WasMouseButtonReleased(Uint8 button) const;

SDL_FPoint MousePosition() const;
SDL_FPoint MouseDeltaThisFrame() const;
SDL_FPoint MouseWheelThisFrame() const;

    void StartTextInput(SDL_Window* window);
    void StopTextInput(SDL_Window* window);
bool IsTextInputActive() const;

const std::string& TextInputThisFrame() const;

private:
    static constexpr std::size_t kMouseButtonCount = 6;

    std::array<bool, SDL_SCANCODE_COUNT> current{};
    std::array<bool, SDL_SCANCODE_COUNT> previous{};

    std::array<bool, kMouseButtonCount> mouse_current{};
    std::array<bool, kMouseButtonCount> mouse_previous{};
    SDL_FPoint mouse_position{};
    SDL_FPoint mouse_delta_this_frame{};
    SDL_FPoint mouse_wheel_this_frame{};

    bool text_input_active = false;
    std::string text_input_this_frame;
};

} // namespace svanes
