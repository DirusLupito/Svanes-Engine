#include "grindset.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/input/input_manager.hpp>

namespace grindset {

void Grindset::Initialize(svanes::GameContext &context) {
    context.camera.x = 960.0F;
    context.camera.y = 540.0F;
    road.Initialize(context.world);
}

void Grindset::Update(const svanes::FrameContext &frame) {
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }

    const float seconds = static_cast<float>(frame.real_delta_tics) /
                          static_cast<float>(svanes::TicsPerSecond);
    road.Update(frame.world, seconds);
}

bool Grindset::ShouldQuit() const { return quit; }

svanes::TicCount Grindset::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace grindset
