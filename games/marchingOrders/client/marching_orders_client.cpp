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
#include <svanes/tilemaps/tilemap.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace marching_orders {

// Size of a unit icon on the map, in world units.
constexpr svanes::Rectangle2D kUnitIconGeometry{.width = 17.5F,
                                                .height = 8.75F};

// Rotation from the unit's heading to the icon's long edge, in radians.
constexpr float kUnitIconRotationOffset = std::numbers::pi_v<float> / 2.0F;

// Camera travel in screen pixels per second while an arrow key is held.
constexpr float kKeyboardPanPixelsPerSecond = 480.0F;

// Smallest magnification allowed relative to one in which the whole map fits in
// the output window.
constexpr double kMinimumZoomFactor = 0.0001;

// Largest magnification allowed relative to one in which the whole map fits in
// the output window.
constexpr double kMaximumZoomFactor = 100.0;

// Initial rate of change of log magnification per second of held input.
constexpr double kInitialZoomRate = 0.5;

// Exponential growth rate per second while a zoom key remains held.
constexpr double kZoomRateGrowth = 1.0;

void MarchingOrdersClient::Initialize(svanes::ClientContext &context) {
    const svanes::ImageData image = LoadTerrainImage();
    terrain_side = static_cast<float>(image.width);
    const auto terrain = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(terrain);
    context.world.AddComponent<svanes::Sprite>(
        terrain, svanes::Sprite{.texture = context.assets.CreateTexture(image),
                                .geometry = {.width = terrain_side,
                                             .height = terrain_side}});
    const svanes::ImageData control_colors{
        .width = 3,
        .height = 1,
        .rgba_pixels = {255, 0, 0, 24, 255, 0, 0, 48, 255, 0, 0, 72}};
    control_atlas = context.assets.CreateTexture(control_colors);
    control_map_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(control_map_entity);
    context.world.AddComponent<svanes::ZOrder>(control_map_entity,
                                               svanes::ZOrder{1});
    // Temporary unit created for testing. To be removed once actual gameplay is added.
    unit_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        unit_entity, svanes::Transform{.rotation = kUnitIconRotationOffset});
    context.world.AddComponent<svanes::Sprite>(
        unit_entity,
        svanes::Sprite{.texture = context.assets.LoadTexture(svanes::AssetPath(
                           "assets/marchingOrders/sprites/infantry_01.png")),
                       .geometry = kUnitIconGeometry});
    context.world.AddComponent<svanes::ZOrder>(unit_entity, svanes::ZOrder{2});
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

/**
 * Moves the camera in the arrow-key direction and lets a middle-button drag
 * move the terrain with the cursor.
 *
 * @param context The camera and current input state.
 * @param seconds Unscaled time elapsed since the previous frame, in seconds.
 */
static void PanCamera(const svanes::ClientFrameContext &context,
                      float seconds) {
    const svanes::Vector2D direction{
        static_cast<float>(context.input.IsDown(svanes::Key::Right) -
                           context.input.IsDown(svanes::Key::Left)),
        static_cast<float>(context.input.IsDown(svanes::Key::Down) -
                           context.input.IsDown(svanes::Key::Up))};

    svanes::Vector2D pan_pixels{};
    const float length = std::hypot(direction.x, direction.y);
    if (length > 0.0F) {
        pan_pixels =
            direction * (kKeyboardPanPixelsPerSecond * seconds / length);
    }

    if (context.input.IsMouseButtonDown(svanes::MouseButton::Middle) &&
        !context.input.WasMouseButtonPressed(svanes::MouseButton::Middle)) {
        pan_pixels -= context.input.MouseDeltaThisFrame();
    }

    const auto pan_world =
        pan_pixels / (context.camera.zoom * context.camera.Scale());
    context.camera.x += pan_world.x;
    context.camera.y += pan_world.y;
}

void MarchingOrdersClient::UpdateCamera(
    const svanes::ClientFrameContext &context) {
    const auto side = std::min(context.output_width, context.output_height);
    if (side <= 0) {
        return;
    }
    const double fit_zoom = static_cast<double>(side) / terrain_side;
    context.camera.zoom = static_cast<float>(fit_zoom * zoom_factor);

    const double seconds =
        static_cast<double>(context.real_delta_tics) / svanes::TicsPerSecond;
    // We move the grabbed point under the cursor before using it as the zoom
    // anchor.
    PanCamera(context, static_cast<float>(seconds));

    const auto direction =
        static_cast<std::int8_t>(context.input.IsDown(svanes::Key::Equals) -
                                 context.input.IsDown(svanes::Key::Minus));

    if (direction != zoom_direction) {
        zoom_direction = direction;
        zoom_hold_seconds = 0.0;
    }

    if (direction == 0 ||
        (direction > 0 && zoom_factor >= kMaximumZoomFactor) ||
        (direction < 0 && zoom_factor <= kMinimumZoomFactor)) {
        return;
    }

    // Integrate the accelerating zoom rate over this frame's elapsed time.
    const double change =
        kInitialZoomRate * std::exp(kZoomRateGrowth * zoom_hold_seconds) *
        std::expm1(kZoomRateGrowth * seconds) / kZoomRateGrowth;
    zoom_hold_seconds += seconds;

    zoom_factor = std::clamp(zoom_factor * std::exp(direction * change),
                             kMinimumZoomFactor, kMaximumZoomFactor);

    context.camera.SetZoomAt(static_cast<float>(fit_zoom * zoom_factor),
                             context.input.MousePosition());
}

void MarchingOrdersClient::Update(const svanes::ClientFrameContext &context) {
    UpdateCamera(context);
    // Apply the latest world state received from the server.
    const auto messages = context.network.PollMessages();
    if (!messages.empty()) {
        svanes::MessageReader reader(messages.back());
        const svanes::Transform icon_transform{
            .x = reader.ReadFloat32(),
            .y = reader.ReadFloat32(),
            .rotation = reader.ReadFloat32() + kUnitIconRotationOffset};
        const auto columns = reader.ReadUint32();
        const auto rows = reader.ReadUint32();
        const auto cell_size = reader.ReadUint32();
        const svanes::Vector2D origin{reader.ReadFloat32(),
                                      reader.ReadFloat32()};
        const auto cell_count = static_cast<std::uint64_t>(columns) * rows;
        if (!std::isfinite(icon_transform.x) ||
            !std::isfinite(icon_transform.y) ||
            !std::isfinite(icon_transform.rotation) ||
            !std::isfinite(origin.x) || !std::isfinite(origin.y) ||
            columns == 0 || rows == 0 || cell_size == 0 ||
            cell_count != reader.Remaining()) {
            throw std::invalid_argument("Invalid world state from server.");
        }
        if (!context.world.HasComponent<svanes::TileMap>(control_map_entity)) {
            context.world.AddComponent<svanes::TileMap>(control_map_entity);
        }
        // Update the tilemap with the received control values
        auto &overlay =
            context.world.GetComponent<svanes::TileMap>(control_map_entity);
        overlay.atlas = control_atlas;
        overlay.columns = columns;
        overlay.rows = rows;
        overlay.tile_width = cell_size;
        overlay.tile_height = cell_size;
        overlay.atlas_tile_width = 1;
        overlay.atlas_tile_height = 1;
        overlay.atlas_columns = 3;
        overlay.atlas_rows = 1;
        overlay.cells.resize(static_cast<std::size_t>(cell_count));
        for (auto &cell : overlay.cells) {
            const auto tier = reader.ReadUint8();
            if (tier > 3) {
                throw std::invalid_argument(
                    "Invalid team control tier from server.");
            }
            cell.tile_id = tier;
        }
        auto &overlay_transform =
            context.world.GetComponent<svanes::Transform>(control_map_entity);
        overlay_transform.x = origin.x;
        overlay_transform.y = origin.y;
        context.world.GetComponent<svanes::Transform>(unit_entity) =
            icon_transform;
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
