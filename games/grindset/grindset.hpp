#pragma once

#include "car_interior.hpp"
#include "phone.hpp"
#include "road.hpp"

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/game.hpp>

namespace grindset {

/**
 * The grindset game: an endless sunrise drive with a phone in hand.
 */
class Grindset final : public svanes::IGame {
public:
    /**
     * Centers the camera so world coordinates match screen pixels, loads the
     * game's font, then sets up the game's views.
     * @param context The engine services and window dimensions.
     */
    void Initialize(svanes::GameContext &context) override;

    /**
     * Checks whether Escape was pressed to quit and whether Space or Tab was
     * pressed to put away or take out the phone, then moves the road, bobs
     * the car interior along with it, and slides the phone.
     * @param frame The current frame's input and engine services.
     */
    void Update(const svanes::FrameContext &frame) override;

    /**
     * Returns whether the player has asked to quit.
     * @return True once Escape has been pressed, false otherwise.
     */
    bool ShouldQuit() const override;

    /**
     * Limits the game to approximately sixty frames per second.
     * @return The interval between frames, in microseconds.
     */
    svanes::TicCount GetFrameIntervalTics() const override;

private:
    Road road;
    CarInterior interior;
    Phone phone;
    svanes::FontHandle font{};
    bool quit = false;
};

} // namespace grindset
