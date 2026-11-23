#pragma once

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/registry.hpp>
#include <svanes/vector2d.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace grindset {

/**
 * The phone held up on the right side of the view. It is a dark body with
 * rounded corners, a screen inset inside the bezel, and a small camera hole at
 * the top of the screen, with a status bar across the top of the screen showing
 * the clock, always 5:30am, and the player's balance. It is drawn above the
 * road and the car interior, and is large, since it is what the player
 * interacts with.
 *
 * The screen shows the home screen: a wallpaper fading from top to bottom and
 * two app icons, Casino and Instagram, with their names below them. An icon
 * darkens while it is being clicked, and opens its app when the click is
 * released over it: a shape in the icon's color grows from the icon to fill
 * the screen, then the app fades in. The apps themselves draw their own
 * content, using AppShown and AppArea. While an app is open the home screen is
 * hidden, and the status bar stays on top. A home bar along the bottom of an
 * open app shows where to swipe up to close it: the app follows the swipe,
 * shrinking back toward its icon, and on release either closes or springs back
 * to full screen depending on how far and how fast it was swiped.
 *
 * The phone is either held up or put away in the pocket, fully below the
 * bottom of the view, and eases between the two when toggled. It starts in the
 * pocket. While it is all the way in the pocket, a hint near where it comes up
 * tells the player how to take it out. While held, it sways gently as if in an
 * unsteady hand, without tilting, since text on it cannot rotate. The sway
 * fades in and out with the slide.
 *
 * Every part of the phone shares one Transform at the phone's center, with its
 * geometry placed relative to that center, so the whole phone moves by moving
 * that one position.
 */
class Phone final {
public:
    /**
     * The apps on the home screen, in the order of their icons.
     *
     * VALUES:
     * - Casino: The live dealer blackjack table.
     * - Instagram: The reels.
     */
    enum class App : std::uint8_t {
        Casino,
        Instagram,
    };

    /**
     * The lowest z order an app draws its content at. Apps use z orders from
     * this up to 9 above it, which draw above the app's background color and
     * below the home bar and the status bar.
     */
    static constexpr std::int32_t kAppZ = 110;

    /**
     * Creates the body, the screen with its wallpaper and app icons, the
     * camera hole, and the status bar in the pocket, and the hint for taking
     * the phone out.
     * @param world The world to create them in. World coordinates are expected
     * to match screen pixels on a 1920x1080 view.
     * @param hint_font The font to write the hint in.
     * @param status_font The smaller font to write the status bar and the
     * app names in.
     */
    void Initialize(svanes::Registry &world, svanes::FontHandle hint_font,
                    svanes::FontHandle status_font);

    /**
     * Starts moving the phone to the pocket if it is held or headed there,
     * or up into the hand otherwise. A slide already underway turns around
     * from where it is.
     */
    void Toggle();

    /**
     * Moves the phone along its slide toward the pocket or the hand, shows
     * the hint only while the phone is all the way in the pocket, shows the
     * balance in the status bar, darkens an app icon while it is clicked, and
     * opens, swipes, and draws the app. Clicks and swipes only start while the
     * phone is all the way up in the hand.
     * @param world The world holding it.
     * @param input The mouse state, with positions matching world
     * coordinates.
     * @param seconds The time since the previous update, in seconds.
     * @param balance_cents The player's balance, in cents. Must not be
     * negative.
     */
    void Update(svanes::Registry &world, const svanes::InputManager &input,
                float seconds, std::int64_t balance_cents);

    /**
     * Returns where the phone's screen is, for apps to lay themselves out in.
     * @return The screen's bounds, ignoring its rounded corners.
     */
    svanes::Rectangle2D ScreenRectangle() const;

    /**
     * Returns how much of an app's content should be showing: none while the
     * app is closed or still growing out of its icon, fading in after that,
     * and fading out early in a swipe to close it.
     * @param app The app.
     * @return From 0 for hidden to 1 for fully shown.
     */
    float AppShown(App app) const;

    /**
     * Returns the area the open app currently covers, which is smaller than
     * the screen while it grows out of or shrinks back into its icon. Apps
     * clip their content to it.
     * @return The area's bounds, ignoring its rounded corners.
     */
    svanes::Rectangle2D AppArea() const;

private:
    /**
     * What the open app is doing.
     *
     * VALUES:
     * - Opening: Growing out of its icon, then fading in.
     * - Open: Filling the screen, waiting for a swipe.
     * - Dragging: Following a swipe up from the bottom of the screen.
     * - Settling: Springing to full screen or back into its icon after a
     * swipe is released.
     */
    enum class AppMotion : std::uint8_t {
        Opening,
        Open,
        Dragging,
        Settling,
    };

    /**
     * Where the mouse was at one moment of a swipe, used to measure how fast
     * the swipe is moving.
     *
     * FIELDS:
     * - seconds: When the position was recorded, as the phone's elapsed time.
     * - y: The mouse's height on the screen.
     */
    struct SwipeSample {
        float seconds;
        float y;
    };

    /**
     * Puts the phone where its slide currently has it, eased so it starts and
     * stops gently, plus as much of the hand's sway as the slide allows. The
     * status bar and app names move along with it.
     * @param world The world holding it.
     */
    void Place(svanes::Registry &world);

    /**
     * Creates the wallpaper: bands fading from the top color to the bottom
     * color, with the rounded top and bottom of the screen in the two end
     * colors.
     * @param world The world to create it in.
     */
    void CreateWallpaper(svanes::Registry &world);

    /**
     * Creates the app icons and their names.
     * @param world The world to create them in.
     * @param font The font to write the names in.
     */
    void CreateIcons(svanes::Registry &world, svanes::FontHandle font);

    /**
     * Starts or ends a click on an app icon, opening the app if the click is
     * released over the icon it started on.
     * @param input The mouse state, with positions matching world
     * coordinates.
     */
    void TrackClick(const svanes::InputManager &input);

    /**
     * Finds the app icon under the mouse.
     * @param input The mouse state, with positions matching world
     * coordinates.
     * @return The icon's index, or nothing if the mouse is not over an icon.
     */
    std::optional<std::size_t>
    IconUnderMouse(const svanes::InputManager &input) const;

    /**
     * Starts, follows, and releases a swipe up from the bottom of an open
     * app. On release, the app closes if the swipe would reach far enough up
     * the screen soon at the speed it was moving, and springs back otherwise.
     * @param input The mouse state, with positions matching world
     * coordinates.
     */
    void TrackSwipe(const svanes::InputManager &input);

    /**
     * Moves the open app along its opening or its spring after a swipe, and
     * closes it once it has sprung back into its icon.
     * @param seconds The time since the previous update, in seconds.
     */
    void MoveApp(float seconds);

    /**
     * Shows the home screen or the open app: hides the icons while an app is
     * open, sizes the app's shape between its icon and the full screen, and
     * fades the app and its home bar in and out.
     * @param world The world holding them.
     */
    void ShowScreen(svanes::Registry &world);

    std::vector<svanes::Entity> parts;
    svanes::Entity hint{};
    svanes::Entity clock{};
    svanes::Entity balance{};
    std::array<svanes::Entity, 2> icons{};
    std::array<svanes::Entity, 2> icon_names{};
    std::optional<std::size_t> pressed_icon;
    std::optional<std::size_t> open_app;
    float open_seconds = 0.0F;
    AppMotion app_motion = AppMotion::Opening;
    float app_amount = 0.0F;
    float app_speed = 0.0F;
    float app_target = 1.0F;
    float swipe_start_y = 0.0F;
    std::vector<SwipeSample> swipe_samples;
    svanes::Entity app_cover{};
    float app_shown = 0.0F;
    svanes::Rectangle2D app_area{};
    svanes::Entity home_bar{};
    svanes::Vector2D position{};
    float slide = 0.0F;
    float elapsed = 0.0F;
    bool held = false;
};

} // namespace grindset
