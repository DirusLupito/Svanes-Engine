#include "network_protocol.hpp"

#include <svanes/network/udp_msg_pipe.hpp>

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <stdexcept>

namespace {

struct BridgeKeys {
    bool character_left = false;
    bool character_right = false;
    bool platform_left = false;
    bool platform_right = false;
    bool platform_up = false;
    bool platform_down = false;
    bool character_jump_pending = false;
    bool platform_action_pending = false;
};

void SetKey(BridgeKeys &keys, SDL_Keycode key, bool down, bool repeat) {
    if (key == SDLK_A) keys.character_left = down;
    if (key == SDLK_D) keys.character_right = down;
    if (key == SDLK_LEFT) keys.platform_left = down;
    if (key == SDLK_RIGHT) keys.platform_right = down;
    if (key == SDLK_UP) keys.platform_up = down;
    if (key == SDLK_DOWN) keys.platform_down = down;
    if (down && !repeat && key == SDLK_W) {
        keys.character_jump_pending = true;
    }
    if (down && !repeat && key == SDLK_SPACE) {
        keys.platform_action_pending = true;
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc > 3) {
        throw std::invalid_argument(
            "Usage: svanes_game_double_time_input_bridge "
            "[character-port] [platform-port]");
    }
    const auto parse_port = [](const char *text, std::uint16_t fallback) {
        if (text == nullptr) return fallback;
        const int value = std::stoi(text);
        if (value <= 0 || value > 65535) {
            throw std::invalid_argument("Bridge ports must be between 1 and 65535.");
        }
        return static_cast<std::uint16_t>(value);
    };
    const std::uint16_t character_port =
        parse_port(argc > 1 ? argv[1] : nullptr,
                   kDoubleTimeInputBridgeCharacterPort);
    const std::uint16_t platform_port =
        parse_port(argc > 2 ? argv[2] : nullptr,
                   kDoubleTimeInputBridgePlatformPort);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, "1");
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Double Time Input Bridge", 520, 160, 0,
                                    &window, &renderer)) {
        const std::string error = SDL_GetError();
        SDL_Quit();
        throw std::runtime_error(error);
    }
    // Keep keyboard focus on the bridge so the two game windows never need to
    // be alternated while testing both roles on one machine.
    if (!SDL_SetWindowFocusable(window, true) ||
        !SDL_SetWindowKeyboardGrab(window, true) || !SDL_RaiseWindow(window)) {
        SDL_Log("Input bridge could not claim keyboard focus: %s", SDL_GetError());
    } else {
        SDL_Log("Input bridge keyboard capture enabled.");
    }

    svanes::UdpMsgPipe pipe;
    pipe.Listen("*:" + std::to_string(kDoubleTimeInputBridgePort));
    const std::string character_destination =
        "127.0.0.1:" + std::to_string(character_port);
    const std::string platform_destination =
        "127.0.0.1:" + std::to_string(platform_port);
    BridgeKeys keys;
    bool running = true;
    auto previous_send = std::chrono::steady_clock::now();
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN ||
                       event.type == SDL_EVENT_KEY_UP) {
                SetKey(keys, event.key.key, event.key.down, event.key.repeat);
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - previous_send >= std::chrono::milliseconds(16)) {
            previous_send = now;
            const DoubleTimeBridgeInput input{
                static_cast<float>(keys.character_right - keys.character_left),
                keys.character_jump_pending,
                keys.platform_left,
                keys.platform_right,
                keys.platform_up,
                keys.platform_down,
                keys.platform_action_pending};
            const svanes::NetworkMessage message =
                svanes::NetworkMessage::From(input);
            pipe.Send(character_destination, message);
            pipe.Send(platform_destination, message);
            keys.character_jump_pending = false;
            keys.platform_action_pending = false;
        }
        SDL_Delay(1);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
