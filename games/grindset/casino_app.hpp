#pragma once

#include "blackjack.hpp"
#include "phone.hpp"

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace grindset {

/**
 * The casino app: a fake live dealer blackjack stream inside the phone screen.
 * The top of the screen shows the stream, with a placeholder dealer standing
 * behind the edge of a green table and a LIVE badge in the corner. Below it
 * are the dealer's cards, the player's cards, the bet, and the controls for
 * building a bet from chips and dealing. The app plays a blackjack round and
 * draws the cards as the round reports them. Everything is laid out against
 * the phone's screen, clipped to the area the app currently covers, and faded
 * in and out with the app.
 */
class CasinoApp final {
public:
    /**
     * Creates the stream, the dealer, the table edge, the LIVE badge, the bet,
     * and the controls, hidden until the app is opened, and starts listening to
     * the round.
     * @param world The world to create them in.
     * @param events Where the round reports what happens.
     * @param font The font to write the badge, the cards, and the controls in.
     * @param phone The phone, to size the layout by.
     */
    void Initialize(svanes::Registry &world, svanes::EventManager &events,
                    svanes::FontHandle font, const Phone &phone);

    /**
     * Moves everything to where the phone's screen is, clips it to the area
     * the app covers, fades it, and handles clicks on the controls.
     * @param world The world holding it.
     * @param input The mouse state, with positions matching world
     * coordinates.
     * @param phone The phone the app is on.
     * @param balance_cents The player's balance, in cents.
     */
    void Update(svanes::Registry &world, const svanes::InputManager &input,
                const Phone &phone, std::int64_t balance_cents);

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

    /**
     * A card on the table.
     *
     * FIELDS:
     * - border: The dark outline around the card.
     * - body: The card's shape.
     * - label: The card's rank and suit, written in its corner.
     * - card: The card.
     * - face_up: Whether the card is showing its face.
     */
    struct CardView {
        svanes::Entity border;
        svanes::Entity body;
        svanes::Entity label;
        blackjack::Card card;
        bool face_up;
    };

    /**
     * A clickable control.
     *
     * FIELDS:
     * - shape: The control's shape.
     * - label: The control's text.
     */
    struct Button {
        svanes::Entity shape;
        svanes::Entity label;
    };

    /**
     * Creates a card for the table, hidden until the next update places it.
     * @param world The world to create it in.
     * @param card The card.
     * @param face_up Whether the card is showing its face.
     * @return The card.
     */
    CardView CreateCard(svanes::Registry &world, blackjack::Card card,
                        bool face_up) const;

    /**
     * Removes every card from the table.
     * @param world The world holding them.
     */
    void ClearCards(svanes::Registry &world);

    /**
     * Returns whether a control can be clicked right now.
     * @param index The control's index.
     * @param balance_cents The player's balance, in cents.
     * @return True if the control is enabled.
     */
    bool IsEnabled(std::size_t index, std::int64_t balance_cents) const;

    /**
     * Does what a control does when it is clicked.
     * @param index The control's index.
     * @param balance_cents The player's balance, in cents.
     */
    void Activate(std::size_t index, std::int64_t balance_cents);

    /**
     * Starts or ends a click on a control, activating it if the click is
     * released over the enabled control it started on.
     * @param input The mouse state, with positions matching world
     * coordinates.
     * @param screen Where the phone's screen is.
     * @param balance_cents The player's balance, in cents.
     */
    void TrackClick(const svanes::InputManager &input,
                    svanes::Rectangle2D screen, std::int64_t balance_cents);

    /**
     * Finds the control under the mouse.
     * @param input The mouse state, with positions matching world
     * coordinates.
     * @param screen Where the phone's screen is.
     * @return The control's index, or nothing if the mouse is not over one.
     */
    std::optional<std::size_t>
    ControlUnderMouse(const svanes::InputManager &input,
                      svanes::Rectangle2D screen) const;

    /**
     * Places and fades one row of cards.
     * @param world The world holding them.
     * @param cards The cards, in the order they were dealt.
     * @param center Where the middle of the row goes.
     * @param area The area the app currently covers.
     * @param shown How much of the app is showing, from 0 to 1.
     */
    void PlaceCards(svanes::Registry &world, const std::vector<CardView> &cards,
                    svanes::Vector2D center, svanes::Rectangle2D area,
                    float shown) const;

    std::vector<Part> parts;
    svanes::Entity live_label{};
    svanes::Entity bet_label{};
    std::array<Button, 7> buttons{};
    std::optional<std::size_t> pressed_button;
    svanes::FontHandle font{};

    std::optional<blackjack::Round> round;
    std::vector<CardView> dealer_cards;
    std::vector<std::vector<CardView>> player_hands;
    std::int64_t bet_cents = 0;
    bool in_play = false;
};

} // namespace grindset
