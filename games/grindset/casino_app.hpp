#pragma once

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <vector>

namespace grindset {

/**
 * The casino app: a fake live dealer blackjack stream inside the phone screen.
 * The top of the screen shows the stream, with a placeholder dealer standing
 * behind the edge of a green table and a LIVE badge in the corner. Everything
 * is laid out against the phone's screen, clipped to the area the app
 * currently covers, and faded in and out with the app.
 */
class CasinoApp final {
public:
    /**
     * Creates the stream, the dealer, the table edge, and the LIVE badge,
     * hidden until the app is opened.
     * @param world The world to create them in.
     * @param font The font to write the LIVE badge in.
     * @param screen Where the phone's screen is, to size the layout by.
     */
    void Initialize(svanes::Registry &world, svanes::FontHandle font,
                    svanes::Rectangle2D screen);

    /**
     * Moves everything to where the phone's screen is, clips it to the area
     * the app covers, and fades it.
     * @param world The world holding it.
     * @param screen Where the phone's screen is.
     * @param area The area the app currently covers.
     * @param shown How much of the app should be showing, from 0 for hidden to
     * 1 for fully shown.
     */
    void Update(svanes::Registry &world, svanes::Rectangle2D screen,
                svanes::Rectangle2D area, float shown);

private:
    /**
     * One shape of the app and the color it has when fully shown.
     *
     * FIELDS:
     * - entity: The shape's entity.
     * - color: The shape's color when the app is fully shown.
     */
    struct Part {
        svanes::Entity entity;
        svanes::Color color;
    };

    std::vector<Part> parts;
    svanes::Entity live_label{};
};

} // namespace grindset
