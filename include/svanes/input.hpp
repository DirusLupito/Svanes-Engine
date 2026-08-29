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

    [[nodiscard]] bool IsDown(SDL_Scancode scancode) const;
    [[nodiscard]] bool WasPressed(SDL_Scancode scancode) const;
    [[nodiscard]] bool WasReleased(SDL_Scancode scancode) const;

    [[nodiscard]] bool IsMouseButtonDown(Uint8 button) const;
    [[nodiscard]] bool WasMouseButtonPressed(Uint8 button) const;
    [[nodiscard]] bool WasMouseButtonReleased(Uint8 button) const;

    [[nodiscard]] SDL_FPoint MousePosition() const;
    [[nodiscard]] SDL_FPoint MouseDeltaThisFrame() const;
    [[nodiscard]] SDL_FPoint MouseWheelThisFrame() const;

    void StartTextInput(SDL_Window* window);
    void StopTextInput(SDL_Window* window);
    [[nodiscard]] bool IsTextInputActive() const;

    [[nodiscard]] const std::string& TextInputThisFrame() const;

private:
    static constexpr std::size_t kMouseButtonCount = 6;

    std::array<bool, SDL_SCANCODE_COUNT> current_{};
    std::array<bool, SDL_SCANCODE_COUNT> previous_{};

    std::array<bool, kMouseButtonCount> mouse_current_{};
    std::array<bool, kMouseButtonCount> mouse_previous_{};
    SDL_FPoint mouse_position_{};
    SDL_FPoint mouse_delta_this_frame_{};
    SDL_FPoint mouse_wheel_this_frame_{};

    bool text_input_active_ = false;
    std::string text_input_this_frame_;
};

} // namespace svanes
