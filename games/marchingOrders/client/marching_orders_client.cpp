#include "marching_orders_client.hpp"
#include "terrain.hpp"

#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/network/clientserver/network_client.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace marching_orders {

void MarchingOrdersClient::Initialize(svanes::ClientContext &context) {
    const svanes::ImageData image = LoadTerrainImage();
    terrain_side = static_cast<float>(image.width);
    const auto terrain = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(terrain);
    context.world.AddComponent<svanes::Sprite>(
        terrain, svanes::Sprite{.texture = context.assets.CreateTexture(image),
                                .geometry = {.width = terrain_side,
                                             .height = terrain_side}});
    // Temporary unit created for testing. To be removed once actual gameplay is added.
    unit_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(unit_entity);
    context.world.AddComponent<svanes::Sprite>(
        unit_entity,
        svanes::Sprite{
            .texture = context.assets.LoadTexture(svanes::AssetPath(
                "assets/marchingOrders/sprites/infantry_01.png")),
            .geometry = svanes::Rectangle2D{.width = 64.0F, .height = 32.0F}});
    context.world.AddComponent<svanes::ZOrder>(unit_entity, svanes::ZOrder{1});
    std::cout << "Hello, world! marchingOrders client.\n" << std::flush;
}

svanes::NetworkMessage MarchingOrdersClient::CreateJoinRequest() { return {}; }

void MarchingOrdersClient::HandleJoinResult(const svanes::JoinResult &result) {
    switch (result.status) {
    case svanes::JoinStatus::Accepted:
        std::cout << "Connected to marchingOrders as client "
                  << result.client_id << ".\n"
                  << std::flush;
        break;
    case svanes::JoinStatus::Rejected:
        throw std::runtime_error(
            "marchingOrders server rejected the join request.");
    case svanes::JoinStatus::TimedOut:
        throw std::runtime_error(
            "marchingOrders server did not answer the join request.");
    }
}

void MarchingOrdersClient::HandleDisconnect() {
    throw std::runtime_error("Lost connection to the marchingOrders server.");
}

void MarchingOrdersClient::Update(const svanes::ClientFrameContext &context) {
    const auto side = std::min(context.output_width, context.output_height);
    if (side > 0) {
        context.camera.zoom = static_cast<float>(side) / terrain_side;
    }
    // Process messages from the server to update entity positions.
    for (const auto &message : context.network.PollMessages()) {
        svanes::MessageReader reader(message);
        const svanes::Vector2D position{reader.ReadFloat32(),
                                        reader.ReadFloat32()};
        if (reader.Remaining() != 0 || !std::isfinite(position.x) ||
            !std::isfinite(position.y)) {
            throw std::invalid_argument("Invalid unit position from server.");
        }
        auto &transform =
            context.world.GetComponent<svanes::Transform>(unit_entity);
        transform.x = position.x;
        transform.y = position.y;
    }
    // Send input over the network on right click. Will need to be updated to allow unit selection.
    if (context.network.IsConnected() &&
        context.input.WasMouseButtonPressed(svanes::MouseButton::Right)) {
        const auto mouse = context.input.MousePosition();
        const auto destination =
            context.camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});
        svanes::MessageWriter writer;
        writer.WriteFloat32(destination.x);
        writer.WriteFloat32(destination.y);
        context.network.Send(writer.Finish());
    }
    if (context.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }
}

bool MarchingOrdersClient::ShouldQuit() const { return quit; }

svanes::TicCount MarchingOrdersClient::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace marching_orders
