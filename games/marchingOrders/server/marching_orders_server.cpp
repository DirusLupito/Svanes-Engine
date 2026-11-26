#include "marching_orders_server.hpp"

#include <svanes/geometry/geometry.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/timeline_system.hpp>

#include <iostream>
#include <stdexcept>

namespace marching_orders {

constexpr float kControlStampValue = 1.0F;
constexpr float kControlStampRadius = 80.0F;
constexpr float kControlHalfLifeSeconds = .75F;
constexpr svanes::TicCount kControlStampInterval = svanes::TicsPerSecond / 16;
constexpr float kLowControlThreshold = 0.25F;
constexpr float kMediumControlThreshold = 1.0F;
constexpr float kHighControlThreshold = 3.0F;

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
    // Update the influence map
    const svanes::Vector2D position{transform.x, transform.y};
    const auto delta_tics =
        context.world.GetComponent<svanes::Timeline>(unit->GetEntity())
            .GetDeltaTics();
    // Fade old values
    team_control_map.Fade(static_cast<float>(delta_tics) /
                              static_cast<float>(svanes::TicsPerSecond),
                          kControlHalfLifeSeconds);
    control_stamp_tics += delta_tics;
    // Stamp new values
    if (!control_stamped || control_stamp_tics >= kControlStampInterval) {
        team_control_map.Stamp(position, kControlStampValue,
                               kControlStampRadius,
                               [](svanes::Vector2D, svanes::Vector2D,
                                  float distance) { return 1.0F - distance; });
        control_stamped = true;
        control_stamp_tics = 0;
    }
    svanes::MessageWriter writer;
    writer.WriteFloat32(transform.x);
    writer.WriteFloat32(transform.y);
    writer.WriteUint32(kControlMapWidth);
    writer.WriteUint32(kControlMapHeight);
    writer.WriteUint32(kControlCellSize);
    writer.WriteFloat32(kControlMapOrigin.x);
    writer.WriteFloat32(kControlMapOrigin.y);
    for (std::uint32_t y = 0; y < kControlMapHeight; ++y) {
        for (std::uint32_t x = 0; x < kControlMapWidth; ++x) {
            const float value = team_control_map.ReadCell(x, y);
            std::uint8_t tier = 0;
            if (value >= kHighControlThreshold) {
                tier = 3;
            } else if (value >= kMediumControlThreshold) {
                tier = 2;
            } else if (value >= kLowControlThreshold) {
                tier = 1;
            }
            writer.WriteUint8(tier);
        }
    }
    context.network.Broadcast(writer.Finish());
}

} // namespace marching_orders
