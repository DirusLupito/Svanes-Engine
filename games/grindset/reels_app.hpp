#pragma once

#include "phone.hpp"

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/registry.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace grindset {

/**
 * The reels app: an endless column of reels, each filling the phone's screen.
 * For now a reel is a placeholder, a solid color with its number written in
 * the middle. The reel being watched fills the screen, with the next reel
 * right below it and the previous one right above it, just out of view.
 *
 * Clicking and dragging up or down anywhere on the screen, except the strip
 * at the bottom used to swipe home, moves the reels along with the mouse, and
 * letting go snaps to whichever reel is closest. The Up and Down arrow keys
 * jump to the previous or next reel. There is always another reel below, and
 * the first reel has nothing above it.
 *
 * Everything is laid out against the phone's screen, clipped to the area the
 * app currently covers, and faded in and out with the app.
 */
class ReelsApp final {
public:
    /**
     * Creates the previous, current, and next reels, hidden until the app is
     * opened.
     * @param world The world to create them in.
     * @param font The font to write the reels' numbers in.
     * @param phone The phone, to size the reels by.
     */
    void Initialize(svanes::Registry &world, svanes::FontHandle font,
                    const Phone &phone);

    /**
     * Moves between reels by dragging or the arrow keys, then moves the reels
     * to where the phone's screen is, clips them to the area the app covers,
     * and fades them with the app. Dragging and the arrow keys only work while
     * the app is fully shown and the phone is fully held, and a drag cut short
     * by either ending snaps to the closest reel.
     * @param world The world holding them.
     * @param input The mouse and keyboard state, with mouse positions
     * matching world coordinates.
     * @param phone The phone the app is on.
     */
    void Update(svanes::Registry &world, const svanes::InputManager &input,
                const Phone &phone);

private:
    /**
     * One of the reels on screen.
     *
     * FIELDS:
     * - card: The reel's shape, the size of the screen.
     * - label: The reel's number, written in the middle.
     */
    struct Slot {
        svanes::Entity card;
        svanes::Entity label;
    };

    /**
     * Starts, follows, and releases a drag on the screen, and jumps to the
     * previous or next reel when the Up or Down arrow key is pressed outside
     * of a drag.
     * @param input The mouse and keyboard state, with mouse positions
     * matching world coordinates.
     * @param screen Where the phone's screen is.
     */
    void TrackInput(const svanes::InputManager &input,
                    svanes::Rectangle2D screen);

    /**
     * Ends a drag, making whichever reel is closest the current one.
     * @param screen_height The height of the phone's screen.
     */
    void Snap(float screen_height);

    std::array<Slot, 3> slots{};
    std::uint32_t current = 0;
    float offset = 0.0F;
    std::optional<float> drag_start_y;
};

} // namespace grindset
