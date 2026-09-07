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

static svanes::Vector2D AttractionField(svanes::Vector2D offset_to_source)
{
    const float distance = std::hypot(offset_to_source.x, offset_to_source.y);
    if (distance == 0.0F) {
        return {};
    }
    const float strength = 1000.0F / ((distance / 100.0F) * (distance / 100.0F));
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
        auto& motion_a = world.GetComponent<svanes::Kinematic2D>(a);
        auto& motion_b = world.GetComponent<svanes::Kinematic2D>(b);
        motion_a.acceleration_x += acceleration.x;
        motion_a.acceleration_y += acceleration.y;
        motion_b.acceleration_x -= acceleration.x;
        motion_b.acceleration_y -= acceleration.y;
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

void OrbitalEscalationGame::Initialize(svanes::GameContext& context)
{
    const svanes::TextureHandle gradient_texture = context.assets.CreateTexture(CreateGradientImage());
    constexpr float square_size = static_cast<float>(kSquarePixels);
    const svanes::Rectangle2D square_geometry{0.0F, 0.0F, square_size, square_size};
    const svanes::Triangle2D triangle_geometry{{{
        {-square_size * 0.5F, square_size / 3.0F},
        {square_size * 0.5F, square_size / 3.0F},
        {0.0F, -square_size * 2.0F / 3.0F},
    }}};
    const svanes::Circle2D circle_geometry{0.0F, 0.0F, square_size * 0.5F};
    const svanes::Rectangle2D view = context.camera.ScreenToWorld({
        context.output_width * 0.5F, context.output_height * 0.5F,
        static_cast<float>(context.output_width), static_cast<float>(context.output_height)
    });

    background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background_entity, svanes::Transform{view.x, view.y}
    );
    context.world.AddComponent<svanes::SolidShape>(
        background_entity,
        svanes::SolidShape{svanes::Color{17, 24, 39, 255},
            svanes::Rectangle2D{0.0F, 0.0F, view.width, view.height}}
    );
    context.world.AddComponent<svanes::ZOrder>(
        background_entity, svanes::ZOrder{-100}
    );

    // first time setup in the middle of the screen
    square_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Collider2D>(square_entity, svanes::Collider2D{square_geometry});
    context.world.AddComponent<svanes::Kinematic2D>(square_entity);
    context.world.AddComponent<svanes::PointAttractor2D>(
        square_entity, svanes::PointAttractor2D{.accelerationField = AttractionField}
    );
    context.world.AddComponent<svanes::Transform>(
        square_entity,
        svanes::Transform{view.x, view.y}
    );
    context.world.AddComponent<svanes::Sprite>(
        square_entity,
        svanes::Sprite{.texture = gradient_texture, .geometry = square_geometry}
    );

    attractor_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Collider2D>(attractor_entity, svanes::Collider2D{square_geometry});
    context.world.AddComponent<svanes::Transform>(
        attractor_entity, svanes::Transform{
            view.x - view.width * 0.25F, view.y
        }
    );
    context.world.AddComponent<svanes::SolidShape>(
        attractor_entity, svanes::SolidShape{svanes::Color{240, 160, 40, 255}, square_geometry}
    );
    context.world.AddComponent<svanes::Kinematic2D>(attractor_entity);

    context.world.GetComponent<svanes::Kinematic2D>(attractor_entity).angular_velocity = 0.5F;

    context.world.AddComponent<svanes::PointAttractor2D>(
        attractor_entity, svanes::PointAttractor2D{.accelerationField = AttractionField}
    );

    triangle_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        triangle_entity, svanes::Transform{view.x + view.width * 0.25F, view.y, 0.4F}
    );
    context.world.AddComponent<svanes::SolidShape>(
        triangle_entity, svanes::SolidShape{svanes::Color{80, 200, 120, 255}, triangle_geometry}
    );
    context.world.AddComponent<svanes::Kinematic2D>(triangle_entity);
    context.world.AddComponent<svanes::PointAttractor2D>(
        triangle_entity, svanes::PointAttractor2D{.accelerationField = AttractionField}
    );
    context.world.AddComponent<svanes::Collider2D>(triangle_entity, svanes::Collider2D{triangle_geometry});
    context.world.GetComponent<svanes::Kinematic2D>(triangle_entity).angular_velocity = -0.5F;

    circle_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        circle_entity, svanes::Transform{view.x, view.y + view.height * 0.25F}
    );
    context.world.AddComponent<svanes::SolidShape>(
        circle_entity, svanes::SolidShape{svanes::Color{255, 0, 0, 255}, circle_geometry}
    );
    context.world.AddComponent<svanes::Kinematic2D>(circle_entity);
    context.world.AddComponent<svanes::PointAttractor2D>(
        circle_entity, svanes::PointAttractor2D{.accelerationField = AttractionField}
    );
    context.world.AddComponent<svanes::Collider2D>(circle_entity, svanes::Collider2D{circle_geometry});
}

void OrbitalEscalationGame::Update(const svanes::FrameContext& frame)
{
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        should_quit = true;
    }

    // 1.1^delta
    // Rolling harder on the mouse wheel will zoom in and out 
    // faster compared to rolling the same distance slowly.
    const float zoom = std::clamp(
        frame.camera.zoom * std::pow(1.1F, frame.input.MouseWheelThisFrame().y), 0.1F, 10.0F
    );
    frame.camera.SetZoomAt(zoom, frame.input.MousePosition());

    constexpr float camera_speed = 300.0F;
    frame.camera.x += camera_speed * frame.delta_seconds * (
        frame.input.IsDown(svanes::Key::Right) - frame.input.IsDown(svanes::Key::Left)
    );
    frame.camera.y += camera_speed * frame.delta_seconds * (
        frame.input.IsDown(svanes::Key::Down) - frame.input.IsDown(svanes::Key::Up)
    );

    const svanes::Rectangle2D view = frame.camera.ScreenToWorld({
        frame.output_width * 0.5F, frame.output_height * 0.5F,
        static_cast<float>(frame.output_width), static_cast<float>(frame.output_height)
    });
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

    for (svanes::Entity entity : {attractor_entity, triangle_entity, circle_entity}) {
        auto& other_motion = frame.world.GetComponent<svanes::Kinematic2D>(entity);
        other_motion.acceleration_x = 0.0F;
        other_motion.acceleration_y = 0.0F;
    }

    ApplyCollisionAcceleration(frame.world, square_entity, attractor_entity);
    ApplyCollisionAcceleration(frame.world, triangle_entity, attractor_entity);
    ApplyCollisionAcceleration(frame.world, triangle_entity, square_entity);
    ApplyCollisionAcceleration(frame.world, circle_entity, square_entity);
    ApplyCollisionAcceleration(frame.world, circle_entity, attractor_entity);
    ApplyCollisionAcceleration(frame.world, circle_entity, triangle_entity);

}

bool OrbitalEscalationGame::ShouldQuit() const
{
    return should_quit;
}
