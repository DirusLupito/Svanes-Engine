#include "marching_orders_server.hpp"

#include <svanes/physics_system.hpp>

#include <iostream>
#include <stdexcept>

namespace marching_orders {

// Travel speed of the starting unit in world units per second.
constexpr float kStartingUnitSpeed = 80.0F;

void MarchingOrdersServer::Initialize(svanes::ServerContext &context) {
    scenario.emplace(Terrain::Get(), context.world);
    scenario->SpawnUnit({}, kStartingUnitSpeed);
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
    for (const auto &input : context.network.PollInputs()) {
        try {
            const auto order = MoveOrder::Decode(input.message);
            if (!scenario->MoveUnit(order)) {
                std::cerr << "Client " << input.client
                          << " ordered an unknown unit " << order.unit << ".\n";
            }
        } catch (const std::invalid_argument &error) {
            std::cerr << "Invalid order from client " << input.client << ": "
                      << error.what() << '\n';
        }
    }
}

void MarchingOrdersServer::PhysicsUpdate(
    const svanes::ServerPhysicsContext &context) {
    scenario->Advance(svanes::DefaultPhysicsStepTics);
    context.network.Broadcast(scenario->MakeSnapshot());
}

} // namespace marching_orders
