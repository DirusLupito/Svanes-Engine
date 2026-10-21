#pragma once

#include <svanes/network/clientserver/network_server.hpp>
#include <svanes/physics_system.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>

namespace svanes {

class NetworkServer;

/**
 * Represents a join request presented to the server for an admission decision.
 *
 * FIELDS:
 * - world: The engine owned registry containing the server's entities.
 * - network: The engine owned network server.
 * - client: The reserved ID, usable for gameplay sends after acceptance.
 * - message: The game specific join request bytes.
 */
struct ServerJoinContext {
    Registry &world;
    NetworkServer &network;
    ClientId client;
    const NetworkMessage &message;
};

/**
 * The game's decision and response to a join request.
 *
 * FIELDS:
 * - accepted: Whether the client may exchange gameplay messages.
 * - message: The game specific acceptance or rejection response.
 */
struct JoinResponse {
    bool accepted = false;
    NetworkMessage message;
};

/**
 * Defines relevant context for a server.
 * For servers to have their network managed by the engine,
 * they must use the NetworkServer provided in this context.
 *
 * FIELDS:
 * - world: The engine owned registry containing the server's entities and
 * components.
 * - network: The engine owned network server used to manage client connections
 * and send/receive messages.
 * - gravity: The engine owned gravity vector applied to entities with
 * Kinematic2D and Gravity components, in world units per local tic squared.
 * - concurrency: The number of worker threads to use for parallel execution.
 * Defaults to 1, which means no parallel execution.
 */
struct ServerContext {
    Registry &world;
    NetworkServer &network;
    Vector2D &gravity;
    std::uint32_t concurrency = 1;
};

/**
 * Defines relevant context for a server update.
 * For servers to have their network managed by the engine,
 * they must use the NetworkServer provided in this context.
 *
 * FIELDS:
 * - world: The engine owned registry containing the server's entities and
 * components.
 * - network: The engine owned network server used to manage client connections
 * and send/receive messages.
 * - gravity: The engine owned gravity vector applied to entities with
 * Kinematic2D and Gravity components, in world units per local tic squared.
 */
struct ServerUpdateContext {
    Registry &world;
    NetworkServer &network;
    Vector2D &gravity;
};

/**
 * Defines relevant context for a server physics update.
 * For servers to have their physics managed by the engine,
 * they must use the NetworkServer provided in this context.
 *
 * FIELDS:
 * - world: The engine owned registry containing the server's entities and
 * components.
 * - network: The engine owned network server used to manage client connections
 * and send/receive messages.
 * - entity_steps: A span of physics time steps for each entity.
 * - parallel_for: The engine owned parallel for driver used to execute tasks in
 * parallel.
 */
struct ServerPhysicsContext {
    Registry &world;
    NetworkServer &network;
    std::span<const PhysicsTimeStep> entity_steps;
    AsyncParallelForDriver &parallel_for;
};

/**
 * Any server that is to be run by the engine must implement this interface.
 * It provides a bridge between the engine and the server, allowing the engine
 * to manage the server's update loop and networking while the server implements
 * its own logic.
 */
class IServer {
public:
    /**
     * Virtual destructor for the IServer interface.
     * Ensures that derived classes can be properly destroyed through a base
     * class pointer.
     */
    virtual ~IServer() = default;

    /**
     * Initializes the server with the provided context.
     * This method is called once at the beginning of the server's lifecycle.
     *
     * @param context The ServerContext containing relevant information for
     * initialization.
     */
    virtual void Initialize(ServerContext &context) = 0;

    /**
     * Examines one single join request and returns the server's decision.
     * Called during network polling, independently of simulation steps.
     *
     * @param context The request, server world, and reserved client identity.
     * @return Whether to admit the client and the game's response bytes.
     */
    virtual JoinResponse HandleJoinRequest(const ServerJoinContext &context) = 0;

    /**
     * Handles a client's disconnect notice or timeout after its session has
     * ended.
     * Called on the main thread during network polling.
     *
     * @param Registry The server's registry containing the client's game
     * objects.
     * @param ClientId The assigned ID of the disconnected client.
     */
    virtual void HandleDisconnect(Registry &, ClientId) {}

    /**
     * Updates the server's state based on the provided context.
     * This method is called once per update tick.
     *
     * @param context The ServerUpdateContext containing relevant information
     * for the update.
     */
    virtual void Update(const ServerUpdateContext &context) = 0;

    /**
     * Updates the server's physics state based on the provided context.
     * This method is called once per update tick.
     *
     * @param context The ServerPhysicsContext containing relevant information
     * for the physics update.
     */
    virtual void PhysicsUpdate(const ServerPhysicsContext &) {}

    /**
     * Determines whether the server should quit its update loop.
     * This method is called once per update tick to check if the server should
     * terminate its execution.
     *
     * @return true if the server should quit, false otherwise.
     */
    virtual bool ShouldQuit() const { return false; }
};

} // namespace svanes
