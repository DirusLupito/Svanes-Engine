#include <svanes/network/server_runtime.hpp>

#include <svanes/timeline_system.hpp>

#include <chrono>
#include <stdexcept>
#include <thread>

namespace svanes {

ServerRuntime::ServerRuntime(NetworkServer &network_server, Registry &world,
                             Vector2D gravity, std::uint32_t concurrency,
                             TicCount physics_step_tics)
    : network_server(network_server), world(world), gravity(gravity),
      parallel_for(concurrency), physics_step_tics(physics_step_tics) {
    if (physics_step_tics <= 0) {
        throw std::invalid_argument("The physics step must be positive.");
    }
}

void ServerRuntime::Run(const MessageHandler &on_message,
                        const BeforePhysicsHandler &before_physics,
                        const PhysicsHandler &after_physics,
                        const StepCompletedHandler &on_step_completed) {
    if (!on_message || !before_physics || !after_physics ||
        !on_step_completed) {
        throw std::invalid_argument("Server runtime callbacks are required.");
    }

    auto previous_tick = std::chrono::steady_clock::now();
    TicCount pending_tics = 0;

    while (true) {
        // Handle incoming network messages first
        for (const NetworkMessage &message : network_server.PollInbound()) {
            on_message(message);
        }

        // Increment tic count
        const auto now = std::chrono::steady_clock::now();
        pending_tics += static_cast<TicCount>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                now - previous_tick)
                .count());
        previous_tick = now;
        
        // Execute pending cycles
        while (pending_tics >= physics_step_tics) {
            before_physics();
            AdvanceTimelines(world, physics_step_tics);
            AdvancePhysics(world, gravity, parallel_for, after_physics);
            on_step_completed();
            pending_tics -= physics_step_tics;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

} // namespace svanes
