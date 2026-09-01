#include "chris_game.hpp"

#include <algorithm>
#include <cmath>

void ChrisGame::OnUpdate(float delta_seconds, const svanes::InputManager& /*input*/)
{
    elapsed_seconds += delta_seconds;
}

void ChrisGame::OnRender(SDL_Renderer* renderer, int output_width, int output_height)
{
    constexpr float square_size = 96.0F;
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
    SDL_SetRenderDrawColor(renderer, 37, 99, 235, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, &square);
}
