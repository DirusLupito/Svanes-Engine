#pragma once

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/network/network_server.hpp>
#include <svanes/physics_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <functional>

namespace svanes {

/**
 * A server loop for handling shared physics between clients.
 * Engine users are expected to use this in tandem with a network
 * server instead of directly implementing the physics loop in their
 * game server code.
 */
class ServerRuntime final {
public:
    /**
     * Function defined by the engine user for handling messages received
     * received from clients. As an example, this function may define how
     * to handle client input and its effects on physics objects.
     */
    using MessageHandler = std::function<void(const NetworkMessage &)>;
    /**
     * Function defined by the engine user to be run before physics steps.
     * For example, it can be used to record positions of entities before
     * physics is applied to calculate distance travelled.
     */
    using BeforePhysicsHandler = std::function<void()>;
    /**
     * Function defined by the engine user to define what happens after each
     * physics step. For example, it can be used for game specific collision
     * resolution.
     */
    using PhysicsHandler =
        std::function<void(std::span<const PhysicsTimeStep>)>;
    /**
     * Function defined by the user to define behavior that should occur once
     * a physics step is completed. For example, broadcasting updated positions
     * to clients.
     */
    using StepCompletedHandler = std::function<void()>;

    /**
     * Cunstructor for a server runtime to be used by a game server.
     * 
     * @param network_server The network server that will handle receiving and
     * sending messages to and from clients.
     * @param world The game registry containing entities that will be affected
     * by the runtime
     * @param gravity The gravity constant to be applied to entities
     * @param concurrency The number of threads that can be created to do physics work
     * @param physics_step_tics The number of tics that pass between physics steps
     */
    ServerRuntime(NetworkServer &network_server, Registry &world,
                  Vector2D gravity, std::uint32_t concurrency = 1,
                  TicCount physics_step_tics = DefaultPhysicsStepTics);

    /**
     * Runs the loop with the provided handlers.
     * 
     * @param on_message The message handling function as defined by the engine user
     * @param before_physics The pre physics step function as defined by the engine user
     * @param after_physics The post physics step function as defined by the engine user
     * @param on_step_completed The step completion function as definded by the engine user
     */
    void Run(const MessageHandler &on_message,
             const BeforePhysicsHandler &before_physics,
             const PhysicsHandler &after_physics,
             const StepCompletedHandler &on_step_completed);

private:
    NetworkServer &network_server;
    Registry &world;
    Vector2D gravity;
    AsyncParallelForDriver parallel_for;
    TicCount physics_step_tics;
};

} // namespace svanes
