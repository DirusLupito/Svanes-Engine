#include "marching_orders_server.hpp"

#include <svanes/network/message_serialization.hpp>
#include <svanes/physics_system.hpp>

#include <iostream>
#include <stdexcept>

namespace marching_orders {

// Travel speed of the starting unit in world units per second.
constexpr float kStartingUnitSpeed = 80.0F;

void MarchingOrdersServer::Initialize(svanes::ServerContext &context) {
    scenario.emplace(Terrain::Get(), context.world);
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
            svanes::MessageReader reader(input.message);
            switch (static_cast<OrderType>(reader.ReadUint8())) {
            case OrderType::Move: {
                const auto order = MoveOrder::Decode(input.message);
                if (!scenario->MoveUnit(order)) {
                    std::cerr << "Client " << input.client
                              << " ordered an unknown unit " << order.unit
                              << ".\n";
                }
                break;
            }
            case OrderType::Spawn: {
                const auto order = SpawnOrder::Decode(input.message);
                scenario->SpawnUnit(order.position, kStartingUnitSpeed);
                break;
            }
            default:
                throw std::invalid_argument("Unknown order type.");
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
