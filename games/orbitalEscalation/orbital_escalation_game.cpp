#include "orbital_escalation_game.hpp"

#include <svanes/render/render_queue.hpp>
#include <svanes/render/texture_manager.hpp>

#include <algorithm>
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
    gradient_texture = context.assets.CreateTexture(CreateGradientImage());
}

void OrbitalEscalationGame::Update(const svanes::FrameContext& frame)
{
    elapsed_seconds += frame.delta_seconds;
}

void OrbitalEscalationGame::BuildRenderQueue(const svanes::RenderContext& context)
{
    constexpr float square_size = static_cast<float>(kSquarePixels);
    const float available_width = std::max(0.0F, static_cast<float>(context.output_width) - square_size);
    const float normalized_position = (std::sin(elapsed_seconds * 2.0F) + 1.0F) * 0.5F;
    const svanes::Rectangle square{
        normalized_position * available_width,
        (static_cast<float>(context.output_height) - square_size) * 0.5F,
        square_size,
        square_size,
    };

    context.render_queue.Clear(svanes::Color{17, 24, 39, 255});
    context.render_queue.DrawTexture(gradient_texture, square);
}
