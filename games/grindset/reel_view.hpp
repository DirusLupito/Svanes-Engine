#pragma once

#include "reel_generator.hpp"

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace grindset {

/**
 * The fonts reels write in.
 *
 * FIELDS:
 * - small: For hashtags, handles, and footers.
 * - medium: For longer text, such as facts and captions.
 * - large: For headline text, such as POV lines and quotes.
 * - emoji: For emoji, in color.
 */
struct ReelFonts {
    svanes::FontHandle small;
    svanes::FontHandle medium;
    svanes::FontHandle large;
    svanes::FontHandle emoji;
};

/**
 * Draws one reel filling the phone's screen, of any kind. It keeps a fixed set
 * of background bands, shapes, and text labels, and each kind of reel uses
 * them its own way, hiding the ones it does not need. The background is drawn
 * as horizontal bands, fading from the reel's top color to its bottom color,
 * or split in half for a would you rather or a brainrot reel. A brainrot
 * reel's game and an emoji story's emoji move on their own.
 */
class ReelView final {
public:
    /**
     * Creates the bands, shapes, and labels, all hidden until a reel is shown.
     * @param world The world to create them in.
     * @param fonts The fonts to write in.
     * @param screen The phone's screen, to size the reel by.
     */
    void Initialize(svanes::Registry &world, const ReelFonts &fonts,
                    svanes::Rectangle2D screen);

    /**
     * Sets up the bands, shapes, and labels to show a reel.
     * @param world The world holding them.
     * @param reel What the reel shows.
     */
    void Show(svanes::Registry &world, const Reel &reel);

    /**
     * Moves the reel to where it is on the screen, moves its moving parts,
     * clips it to where the area the app covers overlaps the reel itself, so
     * nothing reaching past the reel's edges shows on the reels beside it,
     * and fades it.
     * @param world The world holding it.
     * @param center Where the reel's center is.
     * @param area The area the app currently covers.
     * @param shown How much of the reel is showing, from 0 to 1.
     * @param seconds How long the app has been running, in seconds, to time
     * the moving parts by.
     */
    void Place(svanes::Registry &world, svanes::Vector2D center,
               svanes::Rectangle2D area, float shown, float seconds) const;

private:
    /**
     * One band, shape, or label of the reel.
     *
     * FIELDS:
     * - entity: Its entity.
     * - offset: Where it sits from the reel's center, before any motion.
     * - color: Its color when the reel is fully shown, fully transparent when
     * the reel does not use it.
     */
    struct Piece {
        svanes::Entity entity;
        svanes::Vector2D offset;
        svanes::Color color;
    };

    static constexpr std::size_t kBandCount = 24;
    static constexpr std::size_t kShapeCount = 6;
    static constexpr std::size_t kLabelCount = 4;

    /**
     * Sets up one of the labels.
     * @param world The world holding it.
     * @param index Which label.
     * @param text What it says.
     * @param font The font to write it in.
     * @param offset Where its center sits from the reel's center.
     * @param color Its color when the reel is fully shown.
     * @param wrap_width The width to wrap it at, or 0 not to wrap.
     */
    void SetLabel(svanes::Registry &world, std::size_t index,
                  const std::string &text, svanes::FontHandle font,
                  svanes::Vector2D offset, svanes::Color color,
                  std::int32_t wrap_width);

    /**
     * Sets up one of the shapes.
     * @param world The world holding it.
     * @param index Which shape.
     * @param geometry Its shape, centered on its position.
     * @param offset Where it sits from the reel's center.
     * @param color Its color when the reel is fully shown.
     */
    void SetShape(svanes::Registry &world, std::size_t index,
                  svanes::Geometry2D geometry, svanes::Vector2D offset,
                  svanes::Color color);

    /**
     * Finds how far a moving part has moved from where it sits.
     * @param label True for a label, false for a shape.
     * @param index Which shape or label.
     * @param seconds How long the app has been running, in seconds.
     * @return How far it has moved.
     */
    svanes::Vector2D Motion(bool label, std::size_t index, float seconds) const;

    std::array<Piece, kBandCount> bands{};
    std::array<Piece, kShapeCount> shapes{};
    std::array<Piece, kLabelCount> labels{};
    ReelFonts fonts{};
    float width = 0.0F;
    float height = 0.0F;
    ReelFormat format = ReelFormat::Pov;
};

} // namespace grindset
