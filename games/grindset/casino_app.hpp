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
#include <string>
#include <vector>

namespace grindset {

/**
 * The casino app: a fake live dealer blackjack stream inside the phone screen.
 * The top of the screen shows the stream, with a placeholder dealer standing
 * behind the edge of a green table and a LIVE badge in the corner. Below it
 * are the dealer's cards and the player's cards, each with its total beside
 * it, the bet, and the controls:
 * between rounds, chips for building a bet and a button to deal; during a
 * round, Hit, Stand, Double, and Split. After a split, the two hands sit side
 * by side with a marker under the one being played. The app plays a blackjack
 * round and draws the cards as the round reports them, takes the dealer's
 * turn one card at a time, and shows the round's result in a banner across the
 * middle of the table until the next deal. The dealer narrates the round in
 * captions along the bottom of the stream, which fade out after a few seconds
 * with nothing new to say.
 * Everything is laid out against the phone's screen, clipped to the area the
 * app currently covers, and faded in and out with the app.
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
     * @param large_font The font to write the round's result in.
     * @param phone The phone, to size the layout by.
     */
    void Initialize(svanes::Registry &world, svanes::EventManager &events,
                    svanes::FontHandle font, svanes::FontHandle large_font,
                    const Phone &phone);

    /**
     * Moves everything to where the phone's screen is, clips it to the area
     * the app covers, fades it, handles clicks on the controls, and paces the
     * dealer's turn. The dealer only moves while the app is fully shown.
     * @param world The world holding it.
     * @param input The mouse state, with positions matching world
     * coordinates.
     * @param phone The phone the app is on.
     * @param seconds The time since the previous update, in seconds.
     * @param balance_cents The player's balance, in cents.
     */
    void Update(svanes::Registry &world, const svanes::InputManager &input,
                const Phone &phone, float seconds, std::int64_t balance_cents);

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
     * A result shown after a round: the net result in the banner, or what
     * happened to one hand after a split.
     *
     * FIELDS:
     * - text: What happened, such as "Win +$10.00" or "Bust".
     * - color: The color to write it in.
     */
    struct Result {
        std::string text;
        svanes::Color color;
    };

    /**
     * Starts listening to the round to write the dealer's captions.
     * @param events Where the round reports what happens.
     */
    void SubscribeCaptions(svanes::EventManager &events);

    /**
     * Adds a line to the caption being written this frame. Every line said
     * during one frame is shown together as one caption.
     * @param line The line.
     */
    void Say(const std::string &line);

    /**
     * Writes what the player's hand being played adds up to, such as "You
     * have soft 17." or, after a split, "Hand 2: you have 12.".
     * @param hand_index Which of the player's hands to describe.
     * @return The line.
     */
    std::string PlayerTotalLine(std::size_t hand_index) const;

    /**
     * Writes how a hand turned out in one word, such as "Win" or "Bust".
     * @param outcome How the hand turned out.
     * @param bust Whether the hand went over 21.
     * @return The result to show.
     */
    static Result HandResult(blackjack::Outcome outcome, bool bust);

    /**
     * Writes the net result of the whole round for the banner: what happened
     * and how much money was won or lost across every hand.
     * @param payout_cents How much every hand gave back together, including
     * their bets.
     * @return The result to show.
     */
    Result RoundResult(std::int64_t payout_cents) const;

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
     * Returns whether a control is shown right now: the betting controls
     * between rounds, and the playing controls during one.
     * @param index The control's index.
     * @return True if the control is shown.
     */
    bool IsVisible(std::size_t index) const;

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
     * Places and fades one row of cards, with the total of its face up cards
     * written to the right of it.
     * @param world The world holding them.
     * @param cards The cards, in the order they were dealt.
     * @param total The label to write the row's total in.
     * @param center Where the middle of the row goes.
     * @param area The area the app currently covers.
     * @param shown How much of the app is showing, from 0 to 1.
     */
    void PlaceCards(svanes::Registry &world, const std::vector<CardView> &cards,
                    svanes::Entity total, svanes::Vector2D center,
                    svanes::Rectangle2D area, float shown) const;

    std::vector<Part> parts;
    svanes::Entity live_label{};
    svanes::Entity bet_label{};
    std::array<Button, 11> buttons{};
    svanes::Entity hand_marker{};
    std::array<svanes::Entity, 2> result_labels{};
    svanes::Entity dealer_total{};
    std::array<svanes::Entity, 2> player_totals{};
    svanes::Entity banner_panel{};
    svanes::Entity banner_label{};
    std::optional<Result> banner;
    svanes::Entity caption_strip{};
    svanes::Entity caption_label{};
    std::string caption;
    std::string pending_caption;
    float caption_seconds = 0.0F;
    std::optional<std::size_t> pressed_button;
    svanes::FontHandle font{};

    std::optional<blackjack::Round> round;
    std::vector<CardView> dealer_cards;
    std::vector<std::vector<CardView>> player_hands;
    std::int64_t bet_cents = 0;
    bool in_play = false;
    std::size_t active_hand = 0;
    float dealer_seconds = 0.0F;
    std::vector<std::int64_t> hand_bets;
    std::vector<bool> busted;
    std::vector<Result> results;
};

} // namespace grindset
