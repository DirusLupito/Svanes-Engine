#include "EndScreen.hpp"

#include "BoardView.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/network/clientserver/IClient.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>

namespace ghqcxx {

// Time between one-pixel increases in a square's edge length.
static constexpr svanes::TicCount GrowthInterval = svanes::TicsPerSecond / 60;

// The number of new squares appearing at each growth step.
static constexpr std::size_t SquaresPerStep = 3;

// The time it takes the result label to reach full brightness.
static constexpr svanes::TicCount FadeInDuration = svanes::TicsPerSecond;

EndScreen::EndScreen(svanes::Registry &world, Board::Result result,
                     double transitionSeconds)
    : duration(svanes::SecondsToTics(transitionSeconds)) {
    svanes::TextLabel label{.alignment = svanes::TextAlignment::Center,
                            .visible = false};

    switch (result) {
    case Board::Result::RedVictory:
        label.text = "RED VICTORY";
        label.color = BoardView::GetTeamColor(Team::Red);
        break;
    case Board::Result::BlueVictory:
        label.text = "BLUE VICTORY";
        label.color = BoardView::GetTeamColor(Team::Blue);
        break;
    case Board::Result::Draw:
        label.text = "GAME DRAW";
        label.color = {255, 255, 255};
        break;
    default:
        throw std::invalid_argument(
            "The GHQ end screen needs a finished game.");
    }

    // The engine draws text after world shapes, so hide the board labels
    // before covering the board.
    world.ForEach<svanes::TextLabel>(
        [](svanes::Entity, svanes::TextLabel &text) { text.visible = false; });
    resultLabel = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(resultLabel, label);

    overlay = world.CreateEntity();
    world.AddComponent<svanes::Transform>(overlay);
    world.AddComponent<svanes::ZOrder>(
        overlay, svanes::ZOrder{std::numeric_limits<std::int32_t>::max()});
    world.AddComponent<svanes::SolidShape>(
        overlay, svanes::SolidShape{.color = {0, 0, 0},
                                    .geometry = svanes::CompositeShape2D{}});

    std::mt19937 random{std::random_device{}()};
    std::uniform_real_distribution<float> fraction(0.0F, 1.0F);
    for (svanes::TicCount tic = 0; tic < duration; tic += GrowthInterval) {
        for (std::size_t i = 0; i < SquaresPerStep; ++i) {
            centers.push_back({fraction(random), fraction(random)});
        }
    }
}

void EndScreen::Update(const svanes::ClientFrameContext &frame,
                       const BoardView &view) {
    timeline.Advance(frame.real_delta_tics);
    const auto elapsed = timeline.GetTotalTics();
    const auto viewport = frame.camera.Viewport();
    if (viewport.width <= 0 || viewport.height <= 0) {
        return;
    }

    auto &geometry =
        frame.world.GetComponent<svanes::SolidShape>(overlay).geometry;

    if (elapsed < duration) {
        const auto step = static_cast<std::size_t>(elapsed / GrowthInterval);
        auto &parts = std::get<svanes::CompositeShape2D>(geometry).parts;
        parts.resize((step + 1) * SquaresPerStep);
        for (std::size_t i = 0; i < parts.size(); ++i) {
            // Each square starts one pixel wide and grows from its center.
            const float edge =
                static_cast<float>(step + 1 - i / SquaresPerStep);
            parts[i].shape = frame.camera.ScreenToWorld(
                {centers[i].x * viewport.width, centers[i].y * viewport.height,
                 edge, edge});
        }
        return;
    }

    const float edge = std::max(viewport.width, viewport.height);
    geometry = frame.camera.ScreenToWorld({viewport.x, viewport.y, edge, edge});
    auto &label = frame.world.GetComponent<svanes::TextLabel>(resultLabel);
    label.visible = true;
    label.position = {viewport.width * 0.5F, viewport.height * 0.5F};
    label.font = view.GetLabelFont(
        std::min(viewport.width / 8.0F, viewport.height * 0.5F));

    const auto labelTics = elapsed - duration;
    if (labelTics < FadeInDuration) {
        label.color.alpha =
            static_cast<std::uint8_t>(255 * labelTics / FadeInDuration);
        return;
    }

    // Start the pulse at its peak on the first frame after the fade.
    if (!pulseStart) {
        pulseStart = elapsed;
    }
    const auto phase =
        static_cast<float>((elapsed - *pulseStart) % svanes::TicsPerSecond) /
        static_cast<float>(svanes::TicsPerSecond);
    const float brightness =
        0.5F * (1.0F + std::cos(2.0F * std::numbers::pi_v<float> * phase));
    label.color.alpha =
        static_cast<std::uint8_t>(std::lerp(64.0F, 255.0F, brightness));
}

} // namespace ghqcxx
