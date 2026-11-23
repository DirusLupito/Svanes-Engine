#include "snake.hpp"

#include <svanes/input/input_manager.hpp>

#include <SDL3/SDL.h>

namespace nutritionally_accurate_snake {

void Snake::Initialize(svanes::GameContext &) {}

void Snake::Update(const svanes::FrameContext &frame) {
    quit = frame.input.WasPressed(svanes::Key::Escape) ||
           SDL_HasEvent(SDL_EVENT_QUIT) ||
           SDL_HasEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
}

bool Snake::ShouldQuit() const { return quit; }

svanes::TicCount Snake::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

}
