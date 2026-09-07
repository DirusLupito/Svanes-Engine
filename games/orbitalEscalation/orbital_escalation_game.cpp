#include "orbital_escalation_game.hpp"

#include <svanes/attractor_system.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/input.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr std::int32_t kSquarePixels = 96;
constexpr float kPlanetRadius = 4200.0F;

static svanes::Vector2D AttractionField(svanes::Vector2D offset_to_source)
{
    const float distance = std::hypot(offset_to_source.x, offset_to_source.y);
    if (distance == 0.0F) {
        return {};
    }
    const float strength = 1800.0F / (1.0F + distance / kPlanetRadius);
    return offset_to_source / distance * strength;
}

static void ApplyCollisionAcceleration(svanes::Registry& world, svanes::Entity a, svanes::Entity b)
{
    const auto collisions = svanes::DetectCollisions(
        world.GetComponent<svanes::Collider2D>(a).geometry, world.GetComponent<svanes::Transform>(a),
        world.GetComponent<svanes::Collider2D>(b).geometry, world.GetComponent<svanes::Transform>(b)
    );
    for (const svanes::Collision2D& collision : collisions) {
        const svanes::Vector2D acceleration = collision.normal * 10000.0F;
        if (world.HasComponent<svanes::Kinematic2D>(a)) {
            auto& motion = world.GetComponent<svanes::Kinematic2D>(a);
            motion.acceleration_x += acceleration.x;
            motion.acceleration_y += acceleration.y;
        }
        if (world.HasComponent<svanes::Kinematic2D>(b)) {
            auto& motion = world.GetComponent<svanes::Kinematic2D>(b);
            motion.acceleration_x -= acceleration.x;
            motion.acceleration_y -= acceleration.y;
        }
    }
}

static void ApplyCollisionAcceleration(svanes::Registry& world, const std::vector<svanes::Entity>& entities)
{
    for (std::size_t i = 0; i < entities.size(); ++i) {
        for (std::size_t j = i + 1; j < entities.size(); ++j) {
            ApplyCollisionAcceleration(world, entities[i], entities[j]);
        }
    }
}

svanes::ImageData CreateGradientImage()
{
    svanes::ImageData image{
        .width = kSquarePixels,
        .height = kSquarePixels,
        .rgba_pixels = std::vector<std::uint8_t>(static_cast<std::size_t>(kSquarePixels) * kSquarePixels * 4),
    };

    constexpr float start_r = 0xF0;
    constexpr float start_g = 0xF0;
    constexpr float start_b = 0xF0;
    constexpr float end_r = 0x00;
    constexpr float end_g = 0x00;
    constexpr float end_b = 0xFE;

    // We just linearly interpolate the color from the top-left corner to 
    // the bottom-right corner of the square.

    for (std::int32_t y = 0; y < kSquarePixels; ++y) {
        for (std::int32_t x = 0; x < kSquarePixels; ++x) {
            // Measure distance with the 1-norm.
            // Then we normalize it to the range [0, 1] 
            // so it can be used as a lerp parameter. 
            const float t = static_cast<float>(x + y) / (2.0F * (kSquarePixels - 1));
            const std::size_t offset = (static_cast<std::size_t>(y) * kSquarePixels + x) * 4;
            image.rgba_pixels[offset + 0] = static_cast<std::uint8_t>(std::lerp(start_r, end_r, t));
            image.rgba_pixels[offset + 1] = static_cast<std::uint8_t>(std::lerp(start_g, end_g, t));
            image.rgba_pixels[offset + 2] = static_cast<std::uint8_t>(std::lerp(start_b, end_b, t));
            image.rgba_pixels[offset + 3] = 0xFF;
        }
    }

    return image;
}

static svanes::Entity CreatePlanetLayer(svanes::Registry& world, float radius, svanes::Color color, std::int32_t z_order)
{
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity);
    world.AddComponent<svanes::SolidShape>(
        entity, svanes::SolidShape{color, svanes::Circle2D{0.0F, 0.0F, radius}}
    );
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    return entity;
}

void OrbitalEscalationGame::Initialize(svanes::GameContext& context)
{
    const svanes::TextureHandle gradient_texture = context.assets.CreateTexture(CreateGradientImage());
    constexpr float square_size = static_cast<float>(kSquarePixels);
    const svanes::Rectangle2D square_geometry{0.0F, 0.0F, square_size, square_size};
    const svanes::Transform player_start{0.0F, -kPlanetRadius - 800.0F};
    const svanes::RenderLayout layout = svanes::ComputeRenderLayout(
        context.scale_mode, context.output_width, context.output_height
    );
    context.camera.zoom = 0.2F;
    context.camera.x = player_start.x - (context.output_width * 0.5F - layout.offset.x) / layout.scale / context.camera.zoom;
    context.camera.y = player_start.y - (context.output_height * 0.25F - layout.offset.y) / layout.scale / context.camera.zoom;
    const svanes::Rectangle2D view = context.camera.ScreenToWorld(layout.viewport, layout.scale, layout.offset);

    background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background_entity, svanes::Transform{view.x, view.y}
    );
    context.world.AddComponent<svanes::SolidShape>(
        background_entity,
        svanes::SolidShape{svanes::Color{0, 0, 68, 255},
            svanes::Rectangle2D{0.0F, 0.0F, view.width, view.height}}
    );
    context.world.AddComponent<svanes::ZOrder>(
        background_entity, svanes::ZOrder{-100}
    );

    
    square_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Collider2D>(square_entity, svanes::Collider2D{square_geometry});
    context.world.AddComponent<svanes::Kinematic2D>(square_entity);
    context.world.GetComponent<svanes::Kinematic2D>(square_entity).velocity_x = 3000.0F;
    context.world.AddComponent<svanes::Transform>(
        square_entity,
        player_start
    );
    context.world.AddComponent<svanes::Sprite>(
        square_entity,
        svanes::Sprite{.texture = gradient_texture, .geometry = square_geometry}
    );

    planet_entity = CreatePlanetLayer(context.world, kPlanetRadius, {255, 127, 38, 255}, -3);
    CreatePlanetLayer(context.world, 3900.0F, {185, 122, 87, 255}, -2);
    const svanes::Entity inner_layer = CreatePlanetLayer(context.world, 3750.0F, {127, 127, 127, 255}, -1);
    context.world.AddComponent<svanes::Collider2D>(
        planet_entity, svanes::Collider2D{svanes::Circle2D{0.0F, 0.0F, kPlanetRadius}}
    );
    context.world.AddComponent<svanes::PointAttractor2D>(
        inner_layer, svanes::PointAttractor2D{.accelerationField = AttractionField, .cutoff_radius = std::nullopt}
    );
}

void OrbitalEscalationGame::Update(const svanes::FrameContext& frame)
{
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        should_quit = true;
    }

    if (frame.input.WasPressed(svanes::Key::Tab)) {
        frame.scale_mode = frame.scale_mode == svanes::ScaleMode::Constant
            ? svanes::ScaleMode::Proportional : svanes::ScaleMode::Constant;
    }

    const svanes::RenderLayout layout = svanes::ComputeRenderLayout(
        frame.scale_mode, frame.output_width, frame.output_height
    );

    // 1.1^delta
    // Rolling harder on the mouse wheel will zoom in and out 
    // faster compared to rolling the same distance slowly.
    const float zoom = std::clamp(
        frame.camera.zoom * std::pow(1.1F, frame.input.MouseWheelThisFrame().y), 0.01F, 100.0F
    );
    frame.camera.SetZoomAt(zoom, frame.input.MousePosition(), layout.scale, layout.offset);

    constexpr float camera_speed = 300.0F;
    frame.camera.x += camera_speed * frame.delta_seconds * (
        frame.input.IsDown(svanes::Key::Right) - frame.input.IsDown(svanes::Key::Left)
    );
    frame.camera.y += camera_speed * frame.delta_seconds * (
        frame.input.IsDown(svanes::Key::Down) - frame.input.IsDown(svanes::Key::Up)
    );

    const svanes::Rectangle2D view = frame.camera.ScreenToWorld(layout.viewport, layout.scale, layout.offset);
    svanes::Transform& background = frame.world.GetComponent<svanes::Transform>(background_entity);
    background.x = view.x;
    background.y = view.y;
    svanes::Rectangle2D& background_rectangle = std::get<svanes::Rectangle2D>(frame.world.GetComponent<svanes::SolidShape>(background_entity).geometry);
    background_rectangle.width = view.width;
    background_rectangle.height = view.height;

    svanes::Kinematic2D& motion = frame.world.GetComponent<svanes::Kinematic2D>(square_entity);
    motion.acceleration_x = static_cast<float>(
        100.0f * (frame.input.IsDown(svanes::Key::D) - frame.input.IsDown(svanes::Key::A))
    );
    motion.acceleration_y = static_cast<float>(
        100.0f * (frame.input.IsDown(svanes::Key::S) - frame.input.IsDown(svanes::Key::W))
    );
    motion.angular_acceleration = 100.0F * (
        frame.input.IsDown(svanes::Key::E) - frame.input.IsDown(svanes::Key::Q)
    );

    ApplyCollisionAcceleration(frame.world, {square_entity, planet_entity});

}

bool OrbitalEscalationGame::ShouldQuit() const
{
    return should_quit;
}
