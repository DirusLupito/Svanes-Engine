#include "network_config.hpp"

#include <svanes/render/basic_render_types.hpp>

#include <zmq.hpp>

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

int32_t main()
{
    zmq::context_t context;
    zmq::socket_t state_socket(context, zmq::socket_type::pub);
    state_socket.bind(kChrisServerStateBindEndpoint);
    SDL_Log("Server: bound state broadcast socket to %s.", kChrisServerStateBindEndpoint);

    constexpr float kColorChangeIntervalSeconds = 1.0F;
    constexpr std::array<svanes::Color, 4> kColorCycle{
        svanes::Color{17, 24, 39, 255},
        svanes::Color{127, 29, 29, 255},
        svanes::Color{20, 83, 45, 255},
        svanes::Color{30, 58, 138, 255},
    };

    std::size_t color_index = 0;
    auto previous_tick = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const float elapsed_seconds = std::chrono::duration<float>(now - previous_tick).count();

        if (elapsed_seconds >= kColorChangeIntervalSeconds) {
            previous_tick = now;
            color_index = (color_index + 1) % kColorCycle.size();

            const svanes::Color& next_color = kColorCycle[color_index];
            state_socket.send(zmq::buffer(&next_color, sizeof(next_color)), zmq::send_flags::dontwait);
            SDL_Log(
                "Server: broadcast background color (%u, %u, %u).",
                next_color.red, next_color.green, next_color.blue
            );
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
