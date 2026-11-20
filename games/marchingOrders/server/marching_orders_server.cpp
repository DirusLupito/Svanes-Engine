#include "marching_orders_server.hpp"

#include <iostream>

namespace marching_orders {

void MarchingOrdersServer::Initialize(svanes::ServerContext &) {
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

void MarchingOrdersServer::Update(const svanes::ServerUpdateContext &) {}

} // namespace marching_orders
