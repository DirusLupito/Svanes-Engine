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
#include <vector>

namespace grindset {

/**
 * The reels app: an endless column of reels, each filling the phone's screen.
 * For now a reel is a placeholder, a solid color with its number written in
 * the middle. The reel being watched fills the screen, with the next reel
 * right below it and the previous one right above it, just out of view.
 *
 * Clicking and dragging up or down anywhere on the screen, except the strip
 * at the bottom used to swipe home, moves the reels along with the mouse. On
 * letting go, the reels move on to the next or previous reel if they would
 * pass halfway to it soon at the speed they were moving, and go back to the
 * reel they started on otherwise, springing into place either way. The Up and
 * Down arrow keys spring to the previous or next reel. Clicking while the
 * reels are still springing grabs them where they are. There is always
 * another reel below. The first reel has nothing above it: pulling down on it
 * moves it less the further it is pulled, and it always springs back.
 *
 * Staying on a reel for a few seconds pays $0.50 if no reel this far down
 * has paid yet, and a "+$0.50" popup rises and fades near the top of the
 * screen, above where reels write their text. Time only counts as watching
 * while the app is fully shown and the phone is fully held.
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
     * Moves between reels by dragging or the arrow keys, springs the reels
     * into place, then moves them to where the phone's screen is, clips them
     * to the area the app covers, and fades them with the app. Dragging and
     * the arrow keys only work while the app is fully shown and the phone is
     * fully held, and a drag cut short by either ending is let go where it is.
     * Pays for staying on a new reel long enough, and shows the popup when it
     * does.
     * @param world The world holding them.
     * @param input The mouse and keyboard state, with mouse positions
     * matching world coordinates.
     * @param phone The phone the app is on.
     * @param seconds The time since the previous update, in seconds.
     * @return How much was earned this update, in cents.
     */
    std::int64_t Update(svanes::Registry &world,
                        const svanes::InputManager &input, const Phone &phone,
                        float seconds);

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
     * Where the reels were at one moment of a drag, used to measure how fast
     * the drag is moving them.
     *
     * FIELDS:
     * - seconds: When the position was recorded, as the app's elapsed time.
     * - offset: How far the reels were from resting on the current reel.
     */
    struct DragSample {
        float seconds;
        float offset;
    };

    /**
     * Starts, follows, and releases a drag on the screen, and moves to the
     * previous or next reel when the Up or Down arrow key is pressed outside
     * of a drag.
     * @param input The mouse and keyboard state, with mouse positions
     * matching world coordinates.
     * @param screen Where the phone's screen is.
     */
    void TrackInput(const svanes::InputManager &input,
                    svanes::Rectangle2D screen);

    /**
     * Ends a drag, moving on to the next or previous reel if the reels would
     * pass halfway to it soon at the speed the drag was moving, and keeping
     * that speed for the spring to start from.
     * @param screen_height The height of the phone's screen.
     */
    void Release(float screen_height);

    /**
     * Springs the reels toward the current reel, unless they are being
     * dragged.
     * @param seconds The time since the previous update, in seconds.
     */
    void Settle(float seconds);

    /**
     * Times how long the current reel has been watched, starting over when the
     * current reel changes, and pays once it has been watched long enough if
     * no reel this far down has paid yet, starting the popup.
     * @param seconds The time since the previous update, in seconds.
     * @param watching Whether the reels can be watched right now.
     * @return How much was earned, in cents.
     */
    std::int64_t Watch(float seconds, bool watching);

    /**
     * Rises and fades the popup while it is showing, and hides it otherwise.
     * @param world The world holding it.
     * @param screen Where the phone's screen is.
     * @param area The area the app currently covers.
     * @param shown How much of the app is showing, from 0 to 1.
     * @param seconds The time since the previous update, in seconds.
     */
    void ShowPopup(svanes::Registry &world, svanes::Rectangle2D screen,
                   svanes::Rectangle2D area, float shown, float seconds);

    std::array<Slot, 3> slots{};
    std::uint32_t current = 0;
    float offset = 0.0F;
    float speed = 0.0F;
    float elapsed = 0.0F;
    std::optional<float> drag_anchor_y;
    std::vector<DragSample> drag_samples;
    std::uint32_t watched_reel = 0;
    std::uint32_t first_unpaid_reel = 0;
    float watch_seconds = 0.0F;
    svanes::Entity popup_panel{};
    svanes::Entity popup_label{};
    std::optional<float> popup_seconds;
};

} // namespace grindset
