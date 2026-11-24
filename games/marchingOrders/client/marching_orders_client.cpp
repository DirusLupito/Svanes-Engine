#include "marching_orders_client.hpp"
#include "terrain.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>

#include <algorithm>
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
    if (context.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }
}

bool MarchingOrdersClient::ShouldQuit() const { return quit; }

svanes::TicCount MarchingOrdersClient::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace marching_orders
