#include "marching_orders_client.hpp"
#include "terrain_image.hpp"

#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/network/clientserver/network_client.hpp>
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

// Create an atlas of potential color tiles for rendering mixed/overlapping control.
// This way any color that needs to be represented can have an associated tile ID.
static svanes::ImageData MakeControlAtlas() {
    constexpr auto levels = ControlGrid::kColorLevels;
    const auto &opacities = ControlGrid::kOpacities;
    svanes::ImageData atlas{
        .width = static_cast<std::int32_t>(levels * levels),
        .height = static_cast<std::int32_t>(levels * opacities.size())};
    atlas.rgba_pixels.reserve(ControlGrid::kTileCount * 4);
    for (std::uint32_t red = 0; red < levels; ++red) {
        for (std::uint32_t green = 0; green < levels; ++green) {
            for (std::uint32_t blue = 0; blue < levels; ++blue) {
                for (const auto alpha : opacities) {
                    atlas.rgba_pixels.push_back(red * 255 / (levels - 1));
                    atlas.rgba_pixels.push_back(green * 255 / (levels - 1));
                    atlas.rgba_pixels.push_back(blue * 255 / (levels - 1));
                    atlas.rgba_pixels.push_back(alpha);
                }
            }
        }
    }
    return atlas;
}

void MarchingOrdersClient::Initialize(svanes::ClientContext &context) {
    const svanes::ImageData image = LoadTerrainImage(scenario.GetTerrain());
    const auto terrain = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(terrain);
    context.world.AddComponent<svanes::Sprite>(
        terrain, svanes::Sprite{
                     .texture = context.assets.CreateTexture(image),
                     .geometry = {.width = static_cast<float>(image.width),
                                  .height = static_cast<float>(image.height)}});
    const auto control_colors = MakeControlAtlas();
    const auto &control = scenario.GetControl();
    control_map_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        control_map_entity,
        svanes::Transform{.x = control.origin.x, .y = control.origin.y});
    auto &overlay =
        context.world.AddComponent<svanes::TileMap>(control_map_entity);
    overlay.atlas = context.assets.CreateTexture(control_colors);
    overlay.columns = control.columns;
    overlay.rows = control.rows;
    overlay.tile_width = control.cell_size;
    overlay.tile_height = control.cell_size;
    overlay.atlas_tile_width = 1;
    overlay.atlas_tile_height = 1;
    overlay.atlas_columns = control_colors.width;
    overlay.atlas_rows = control_colors.height;
    overlay.cells.resize(control.tiles.size());
    context.world.AddComponent<svanes::ZOrder>(control_map_entity,
                                               svanes::ZOrder{1});
    unit_texture = context.assets.LoadTexture(
        svanes::AssetPath("assets/marchingOrders/sprites/infantry_01.png"));
    std::cout << "Hello, world! marchingOrders client.\n" << std::flush;
}

svanes::NetworkMessage MarchingOrdersClient::CreateJoinRequest() { return {}; }

void MarchingOrdersClient::HandleJoinResult(const svanes::JoinResult &result) {
    switch (result.status) {
    case svanes::JoinStatus::Accepted:
        client_id = result.client_id;
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
    if (context.output_width <= 0 || context.output_height <= 0) {
        return;
    }
    const auto &terrain = scenario.GetTerrain();
    const double fit_zoom =
        std::min(static_cast<double>(context.output_width) / terrain.Width(),
                 static_cast<double>(context.output_height) / terrain.Height());
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

void MarchingOrdersClient::UpdateScenarioView(svanes::Registry &world) {
    const auto &units = scenario.GetUnits();
    std::erase_if(unit_icons, [&](const auto &icon) {
        if (units.contains(icon.first)) {
            return false;
        }
        world.DestroyEntity(icon.second);
        return true;
    });

    for (const auto &[id, state] : units) {
        auto [icon, inserted] = unit_icons.try_emplace(id);
        if (inserted) {
            icon->second = world.CreateEntity();
            world.AddComponent<svanes::Sprite>(
                icon->second, svanes::Sprite{.texture = unit_texture,
                                             .geometry = kUnitIconGeometry});
            world.AddComponent<svanes::ZOrder>(icon->second, svanes::ZOrder{2});
            world.AddComponent<svanes::Transform>(icon->second);
        }

        auto &pose = world.GetComponent<svanes::Transform>(icon->second);
        pose = state.transform;
        pose.rotation += kUnitIconRotationOffset;
    }

    auto &overlay = world.GetComponent<svanes::TileMap>(control_map_entity);
    const auto &tiles = scenario.GetControl().tiles;
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        overlay.cells[i].tile_id = tiles[i];
    }
}

void MarchingOrdersClient::Update(const svanes::ClientFrameContext &context) {
    UpdateCamera(context);
    const auto messages = context.network.PollMessages();
    if (!messages.empty()) {
        scenario.ApplySnapshot(messages.back());
        UpdateScenarioView(context.world);
    }

    const auto mouse = context.input.MousePosition();
    const auto position =
        context.camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});
    const auto &units = scenario.GetUnits();
    if (selected_unit) {
        const auto unit = units.find(*selected_unit);
        if (unit == units.end() || unit->second.team != client_id) {
            selected_unit.reset();
        }
    }
    if (context.input.WasMouseButtonPressed(svanes::MouseButton::Left)) {
        const auto previous = selected_unit;
        selected_unit.reset();
        for (const auto &[id, icon] : unit_icons) {
            if (units.at(id).team != client_id) {
                continue;
            }
            const auto &pose =
                context.world.GetComponent<svanes::Transform>(icon);
            const float dx = position.x - pose.x;
            const float dy = position.y - pose.y;
            const float cosine = std::cos(pose.rotation);
            const float sine = std::sin(pose.rotation);
            const float local_x = dx * cosine + dy * sine;
            const float local_y = -dx * sine + dy * cosine;
            if (std::abs(local_x) <= kUnitIconGeometry.width * 0.5F &&
                std::abs(local_y) <= kUnitIconGeometry.height * 0.5F) {
                if (previous != id) {
                    selected_unit = id;
                }
                break;
            }
        }
    }
    if (context.network.IsConnected() &&
        context.input.WasPressed(svanes::Key::P)) {
        const SpawnOrder order{.position = {position.x, position.y}};
        context.network.Send(order.Encode());
    }
    if (context.network.IsConnected() && selected_unit &&
        context.input.WasMouseButtonPressed(svanes::MouseButton::Right)) {
        const MoveOrder order{.unit = *selected_unit,
                              .destination = {position.x, position.y}};
        context.network.Send(order.Encode());
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
