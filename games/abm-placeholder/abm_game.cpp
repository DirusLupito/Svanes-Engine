#include "abm_game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr int kSquarePixels = 96;

SDL_Texture* CreateGradientTexture(SDL_Renderer* renderer)
{
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        kSquarePixels,
        kSquarePixels
    );

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kSquarePixels) * kSquarePixels * 4);

    constexpr float start_r = 0xF0;
    constexpr float start_g = 0xF0;
    constexpr float start_b = 0xF0;
    constexpr float end_r = 0x00;
    constexpr float end_g = 0x00;
    constexpr float end_b = 0xFE;

    for (int y = 0; y < kSquarePixels; ++y) {
        for (int x = 0; x < kSquarePixels; ++x) {
            const float t = static_cast<float>(x + y) / (2.0F * (kSquarePixels - 1));
            const std::size_t offset = (static_cast<std::size_t>(y) * kSquarePixels + x) * 4;
            pixels[offset + 0] = static_cast<std::uint8_t>(std::lerp(start_r, end_r, t));
            pixels[offset + 1] = static_cast<std::uint8_t>(std::lerp(start_g, end_g, t));
            pixels[offset + 2] = static_cast<std::uint8_t>(std::lerp(start_b, end_b, t));
            pixels[offset + 3] = 0xFF;
        }
    }

    SDL_UpdateTexture(texture, nullptr, pixels.data(), kSquarePixels * 4);
    return texture;
}

} // namespace

AbmGame::~AbmGame()
{
    if (gradient_texture != nullptr) {
        SDL_DestroyTexture(gradient_texture);
    }
}

void AbmGame::OnUpdate(float delta_seconds, const svanes::InputManager& /*input*/)
{
    elapsed_seconds += delta_seconds;
}

void AbmGame::OnRender(SDL_Renderer* renderer, int output_width, int output_height)
{
    if (gradient_texture == nullptr) {
        gradient_texture = CreateGradientTexture(renderer);
    }

    constexpr float square_size = static_cast<float>(kSquarePixels);
    const float available_width = std::max(0.0F, static_cast<float>(output_width) - square_size);
    const float normalized_position = (std::sin(elapsed_seconds * 2.0F) + 1.0F) * 0.5F;
    const SDL_FRect square{
        normalized_position * available_width,
        (static_cast<float>(output_height) - square_size) * 0.5F,
        square_size,
        square_size,
    };

    SDL_SetRenderDrawColor(renderer, 17, 24, 39, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, gradient_texture, nullptr, &square);
}
