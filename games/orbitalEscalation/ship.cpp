#include "ship.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/input.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr std::int32_t kSquarePixels = 300;

/**
 * Helper function to create an entity with a Timeline component that is a
 * child of the provided gameplay_timeline entity. This is useful for creating
 * entities that should be synchronized with the main gameplay timeline.
 *
 * @param world The registry in which to create the entity.
 * @param gameplay_timeline The parent timeline entity to which the new entity's
 * Timeline will be linked.
 *
 * @return The newly created entity with a Timeline component.
 */
static svanes::Entity CreateTimedEntity(svanes::Registry &world,
                                        svanes::Entity gameplay_timeline) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Timeline>(entity, gameplay_timeline);
    return entity;
}

/**
 * Creates a gradient image of size kSquarePixels x kSquarePixels, where the
 * color transitions from a light color in the top-left corner to a dark color
 * in the bottom-right corner
 *
 * @return An ImageData object containing the generated gradient image.
 */
static svanes::ImageData CreateGradientImage() {
    svanes::ImageData image{
        .width = kSquarePixels,
        .height = kSquarePixels,
        .rgba_pixels = std::vector<std::uint8_t>(
            static_cast<std::size_t>(kSquarePixels) * kSquarePixels * 4),
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
            const float t =
                static_cast<float>(x + y) / (2.0F * (kSquarePixels - 1));
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kSquarePixels + x) * 4;
            image.rgba_pixels[offset + 0] =
                static_cast<std::uint8_t>(std::lerp(start_r, end_r, t));
            image.rgba_pixels[offset + 1] =
                static_cast<std::uint8_t>(std::lerp(start_g, end_g, t));
            image.rgba_pixels[offset + 2] =
                static_cast<std::uint8_t>(std::lerp(start_b, end_b, t));
            image.rgba_pixels[offset + 3] = 0xFF;
        }
    }

    return image;
}

Ship::Ship(svanes::Registry &world, svanes::TextureManager &assets,
           svanes::Entity gameplay_timeline, const svanes::Transform &start)
    : entity(CreateTimedEntity(world, gameplay_timeline)) {
    const svanes::TextureHandle gradient_texture =
        assets.CreateTexture(CreateGradientImage());
    constexpr float square_size = static_cast<float>(kSquarePixels);
    const svanes::Rectangle2D square_geometry{0.0F, 0.0F, square_size,
                                              square_size};
    world.AddComponent<svanes::Collider2D>(entity,
                                           svanes::Collider2D{square_geometry});
    world.AddComponent<svanes::Kinematic2D>(entity);
    world.GetComponent<svanes::Kinematic2D>(entity).velocity_x =
        svanes::PerSecondToPerTic(3000.0F);
    world.AddComponent<svanes::Transform>(entity, start);
    world.AddComponent<svanes::Sprite>(
        entity, svanes::Sprite{.texture = gradient_texture,
                               .geometry = square_geometry});
    world.AddComponent<svanes::RadialGradient2D>(
        entity,
        svanes::RadialGradient2D{
            .geometry = svanes::Circle2D{0.0F, 0.0F, square_size * 5.0F},
            .center_color = svanes::Color{255, 255, 255, 160},
            .edge_color = svanes::Color{64, 128, 255, 0},
        });
}

svanes::Entity Ship::GetEntity() const { return entity; }

void Ship::ApplyInput(svanes::Registry &world,
                      const svanes::InputManager &input) const {
    svanes::Vector2D direction{
        static_cast<float>(input.IsDown(svanes::Key::D) -
                           input.IsDown(svanes::Key::A)),
        static_cast<float>(input.IsDown(svanes::Key::S) -
                           input.IsDown(svanes::Key::W)),
    };
    const float magnitude = std::hypot(direction.x, direction.y);
    if (magnitude > 1.0F) {
        direction /= magnitude;
    }
    const svanes::Vector2D acceleration =
        direction * svanes::PerSecondSquaredToPerTicSquared(max_acceleration);
    auto &motion = world.GetComponent<svanes::Kinematic2D>(entity);
    motion.acceleration_x = acceleration.x;
    motion.acceleration_y = acceleration.y;
    motion.angular_acceleration =
        svanes::PerSecondSquaredToPerTicSquared(max_angular_acceleration) *
        (input.IsDown(svanes::Key::E) - input.IsDown(svanes::Key::Q));
}
