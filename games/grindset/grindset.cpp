#include "grindset.hpp"

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/input/input_manager.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace grindset {

namespace {

constexpr float kFontPoints = 28.0F;
constexpr float kStatusFontPoints = 20.0F;

svanes::FontHandle LoadFont(svanes::FontManager &fonts, const std::string &path,
                            float points) {
    const svanes::FontHandle font = fonts.LoadFont(path, points);
    if (font.id == 0) {
        throw std::runtime_error("Grindset could not load its font from " +
                                 path + ".");
    }
    return font;
}

} // namespace

void Grindset::Initialize(svanes::GameContext &context) {
    context.camera.x = 960.0F;
    context.camera.y = 540.0F;

    const std::string font_path =
        svanes::AssetPath(GRINDSET_ASSETS_DIR) + "/fonts/consola.ttf";
    font = LoadFont(context.fonts, font_path, kFontPoints);
    status_font = LoadFont(context.fonts, font_path, kStatusFontPoints);

    road.Initialize(context.world);
    interior.Initialize(context.world);
    phone.Initialize(context.world, font, status_font);
    casino.Initialize(context.world, context.events, status_font, phone);

    context.events.Subscribe(
        std::make_unique<
            svanes::EventHandlerWrapper<blackjack::BetPlacedEvent>>(
            [this](const blackjack::BetPlacedEvent &event) {
                if (event.bet_cents > balance_cents) {
                    throw std::logic_error(
                        "A bet of " + FormatDollars(event.bet_cents) +
                        " was placed with only " +
                        FormatDollars(balance_cents) + " left.");
                }
                balance_cents -= event.bet_cents;
            }));
    context.events.Subscribe(
        std::make_unique<
            svanes::EventHandlerWrapper<blackjack::RoundFinishedEvent>>(
            [this](const blackjack::RoundFinishedEvent &event) {
                balance_cents += event.payout_cents;
            }));
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
    phone.Update(frame.world, frame.input, seconds, balance_cents);
    casino.Update(frame.world, frame.input, phone, seconds, balance_cents);
}

bool Grindset::ShouldQuit() const { return quit; }

svanes::TicCount Grindset::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace grindset
