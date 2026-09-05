#include "orbital_escalation_game.hpp"

#include <svanes/input.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr std::int32_t kSquarePixels = 96;

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

    background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(background_entity);
    context.world.AddComponent<svanes::SolidRectangle>(
        background_entity,
        svanes::SolidRectangle{svanes::Color{17, 24, 39, 255}}
    );

    square_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Kinematic2D>(square_entity);
    context.world.AddComponent<svanes::Transform>(
        square_entity,
        svanes::Transform{0.0F, 0.0F, square_size, square_size}
    );
    context.world.AddComponent<svanes::Sprite>(
        square_entity,
        svanes::Sprite{.texture = gradient_texture}
    );
}

void OrbitalEscalationGame::Update(const svanes::FrameContext& frame)
{
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        should_quit = true;
    }

    svanes::Transform& background = frame.world.GetComponent<svanes::Transform>(background_entity);
    background.width = static_cast<float>(frame.output_width);
    background.height = static_cast<float>(frame.output_height);

    svanes::Transform& square = frame.world.GetComponent<svanes::Transform>(square_entity);
    // first time setup in the middle of the screen
    if (!square_positioned) {
        square.x = (static_cast<float>(frame.output_width) - square.width) * 0.5F;
        square.y = (static_cast<float>(frame.output_height) - square.height) * 0.5F;
        square_positioned = true;
    }

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
}

bool OrbitalEscalationGame::ShouldQuit() const
{
    return should_quit;
}
