#include "grindset.hpp"

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input/input_manager.hpp>

#include <stdexcept>
#include <string>

namespace grindset {

namespace {

constexpr float kFontPoints = 28.0F;

} // namespace

void Grindset::Initialize(svanes::GameContext &context) {
    context.camera.x = 960.0F;
    context.camera.y = 540.0F;

    const std::string font_path =
        svanes::AssetPath(GRINDSET_ASSETS_DIR) + "/fonts/consola.ttf";
    font = context.fonts.LoadFont(font_path, kFontPoints);
    if (font.id == 0) {
        throw std::runtime_error("Grindset could not load its font from " +
                                 font_path + ".");
    }

    road.Initialize(context.world);
    interior.Initialize(context.world);
    phone.Initialize(context.world, font);
}

void Grindset::Update(const svanes::FrameContext &frame) {
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }

    const float seconds = static_cast<float>(frame.real_delta_tics) /
                          static_cast<float>(svanes::TicsPerSecond);
    if (frame.input.WasPressed(svanes::Key::Space) ||
        frame.input.WasPressed(svanes::Key::Tab)) {
        phone.Toggle();
    }

    road.Update(frame.world, seconds);
    interior.Update(frame.world, road.CurrentBob());
    phone.Update(frame.world, seconds);
}

bool Grindset::ShouldQuit() const { return quit; }

svanes::TicCount Grindset::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace grindset
