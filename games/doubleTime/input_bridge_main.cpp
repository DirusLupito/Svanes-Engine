#include "network_protocol.hpp"

#include <svanes/application.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/game.hpp>
#include <svanes/input/input_events.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/network/udp_msg_pipe.hpp>

#include <SDL3/SDL.h>

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>

class DoubleTimeInputBridge final : public svanes::IGame {
public:
    DoubleTimeInputBridge(std::uint16_t character_port,
                          std::uint16_t platform_port)
        : character_destination("127.0.0.1:" + std::to_string(character_port)),
          platform_destination("127.0.0.1:" + std::to_string(platform_port)) {}

    void Initialize(svanes::GameContext &context) override {
        context.automatic_simulation = false;
        pipe.Listen("*:" + std::to_string(kDoubleTimeInputBridgePort));
        context.events.Subscribe(
            std::make_unique<
                svanes::EventHandlerWrapper<svanes::KeyInputEvent>>(
                [this](const svanes::KeyInputEvent &event) {
                    if (!event.down) {
                        return;
                    }
                    switch (event.key) {
                    case svanes::Key::Left:
                        pending.platform_left = true;
                        break;
                    case svanes::Key::Right:
                        pending.platform_right = true;
                        break;
                    case svanes::Key::Up:
                        pending.platform_up = true;
                        break;
                    case svanes::Key::Down:
                        pending.platform_down = true;
                        break;
                    case svanes::Key::W:
                        pending.character_jump = true;
                        break;
                    case svanes::Key::Space:
                        pending.platform_action = true;
                        break;
                    default:
                        break;
                    }
                }));
        std::int32_t count = 0;
        SDL_Window **windows = SDL_GetWindows(&count);
        if (!windows || count != 1) {
            SDL_free(windows);
            throw std::runtime_error(
                "Cannot identify the Double Time input bridge window.");
        }
        auto *window = windows[0];
        SDL_free(windows);
        if (!SDL_SetWindowFocusable(window, true) ||
            !SDL_SetWindowKeyboardGrab(window, true) ||
            !SDL_RaiseWindow(window)) {
            SDL_Log("Input bridge could not claim keyboard focus: %s",
                    SDL_GetError());
        }
    }

    void Update(const svanes::FrameContext &frame) override {
        SDL_Event event;
        if (frame.input.WasPressed(svanes::Key::Escape) ||
            SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_QUIT,
                           SDL_EVENT_QUIT) > 0 ||
            SDL_PeepEvents(&event, 1, SDL_GETEVENT,
                           SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                           SDL_EVENT_WINDOW_CLOSE_REQUESTED) > 0) {
            should_quit = true;
            return;
        }
        send_tics += frame.real_delta_tics;
        if (send_tics < 16000) {
            return;
        }
        send_tics %= 16000;
        auto sent = pending;
        sent.character_horizontal =
            static_cast<float>(frame.input.IsDown(svanes::Key::D)) -
            static_cast<float>(frame.input.IsDown(svanes::Key::A));
        sent.platform_left |= frame.input.IsDown(svanes::Key::Left);
        sent.platform_right |= frame.input.IsDown(svanes::Key::Right);
        sent.platform_up |= frame.input.IsDown(svanes::Key::Up);
        sent.platform_down |= frame.input.IsDown(svanes::Key::Down);
        const auto message = svanes::NetworkMessage::From(sent);
        pipe.Send(character_destination, message);
        pipe.Send(platform_destination, message);
        pending = {};
    }

    bool ShouldQuit() const override { return should_quit; }
    svanes::TicCount GetFrameIntervalTics() const override { return 1000; }

private:
    svanes::UdpMsgPipe pipe;
    std::string character_destination;
    std::string platform_destination;
    DoubleTimeBridgeInput pending;
    bool should_quit = false;
    svanes::TicCount send_tics = 0;
};

std::int32_t main(std::int32_t argc, char **argv) {
    if (argc > 3) {
        throw std::invalid_argument(
            "Usage: svanes_game_double_time_input_bridge [character-port] "
            "[platform-port]");
    }
    const auto parse_port = [](const char *text, std::uint16_t fallback) {
        if (!text) {
            return fallback;
        }
        const std::string value{text};
        std::uint32_t port = 0;
        const auto result =
            std::from_chars(value.data(), value.data() + value.size(), port);
        if (result.ec != std::errc{} ||
            result.ptr != value.data() + value.size() || port == 0 ||
            port > 65535) {
            throw std::invalid_argument(
                "Bridge ports must be between 1 and 65535.");
        }
        return static_cast<std::uint16_t>(port);
    };
    DoubleTimeInputBridge game(parse_port(argc > 1 ? argv[1] : nullptr,
                                          kDoubleTimeInputBridgeCharacterPort),
                               parse_port(argc > 2 ? argv[2] : nullptr,
                                          kDoubleTimeInputBridgePlatformPort));
    svanes::Application application({"Double Time Input Bridge", 520, 160});
    return application.run(game);
}
