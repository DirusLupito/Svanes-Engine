#include "marching_orders_server.hpp"

#include <svanes/geometry/geometry.hpp>
#include <svanes/network/message_serialization.hpp>

#include <iostream>
#include <stdexcept>

namespace marching_orders {

void MarchingOrdersServer::Initialize(svanes::ServerContext &context) {
    // Test unit, to be removed later.
    unit.emplace(context.world, svanes::Vector2D{}, 80.0F);
    std::cout << "Hello, world! marchingOrders server.\n" << std::flush;
}

svanes::JoinResponse MarchingOrdersServer::HandleJoinRequest(
    const svanes::ServerJoinContext &context) {
    std::cout << "Accepting marchingOrders client " << context.client << ".\n"
              << std::flush;
    return {.accepted = true, .message = {}};
}

void MarchingOrdersServer::HandleDisconnect(svanes::Registry &,
                                            svanes::ClientId client) {
    std::cout << "marchingOrders client " << client << " disconnected.\n"
              << std::flush;
}

void MarchingOrdersServer::Update(const svanes::ServerUpdateContext &context) {
    // Read input messages for updating unit position
    for (const auto &input : context.network.PollInputs()) {
        svanes::MessageReader reader(input.message);
        const svanes::Vector2D destination{reader.ReadFloat32(),
                                           reader.ReadFloat32()};
        if (reader.Remaining() != 0) {
            throw std::invalid_argument(
                "Unexpected data in unit movement request.");
        }
        unit->SetDestination(context.world, destination);
    }
}

// Update the position of the unit, update clients with the updated position.
void MarchingOrdersServer::PhysicsUpdate(
    const svanes::ServerPhysicsContext &context) {
    unit->PhysicsUpdate(context.world);
    const auto &transform =
        context.world.GetComponent<svanes::Transform>(unit->GetEntity());
    svanes::MessageWriter writer;
    writer.WriteFloat32(transform.x);
    writer.WriteFloat32(transform.y);
    context.network.Broadcast(writer.Finish());
}

} // namespace marching_orders
