#include "grindset.hpp"

#include <svanes/input/input_manager.hpp>

namespace grindset {

void Grindset::Initialize(svanes::GameContext &) {}

void Grindset::Update(const svanes::FrameContext &frame) {
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }
}

bool Grindset::ShouldQuit() const { return quit; }

svanes::TicCount Grindset::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace grindset
