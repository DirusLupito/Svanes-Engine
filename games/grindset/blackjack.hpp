#pragma once

#include <svanes/events/event.hpp>
#include <svanes/events/event_manager.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <string_view>
#include <vector>

namespace grindset::blackjack {

/**
 * A playing card's rank.
 *
 * VALUES:
 * - Ace: The ace, worth 11 or 1.
 * - Two: The two.
 * - Three: The three.
 * - Four: The four.
 * - Five: The five.
 * - Six: The six.
 * - Seven: The seven.
 * - Eight: The eight.
 * - Nine: The nine.
 * - Ten: The ten.
 * - Jack: The jack, worth 10.
 * - Queen: The queen, worth 10.
 * - King: The king, worth 10.
 */
enum class Rank : std::uint8_t {
    Ace,
    Two,
    Three,
    Four,
    Five,
    Six,
    Seven,
    Eight,
    Nine,
    Ten,
    Jack,
    Queen,
    King,
};

/**
 * A playing card's suit.
 *
 * VALUES:
 * - Clubs: Clubs.
 * - Diamonds: Diamonds.
 * - Hearts: Hearts.
 * - Spades: Spades.
 */
enum class Suit : std::uint8_t {
    Clubs,
    Diamonds,
    Hearts,
    Spades,
};

/**
 * One playing card.
 *
 * FIELDS:
 * - rank: The card's rank.
 * - suit: The card's suit.
 */
struct Card {
    Rank rank;
    Suit suit;
};

/**
 * Finds what a card counts for in blackjack. Number cards count as printed,
 * face cards count 10, and an ace counts 11. Whether an ace drops to 1 depends
 * on the rest of the hand.
 *
 * @param card The card.
 * @return The card's value.
 */
std::int32_t CardValue(Card card);

/**
 * The dealer's shoe: six standard decks shuffled together, with a cut card
 * about three quarters of the way in. Drawing past the cut card does not stop
 * the round in progress; it marks the shoe as due for a reshuffle before the
 * next round.
 */
class Shoe final {
public:
    /**
     * Fills the shoe with six decks and shuffles them, using a random seed.
     */
    Shoe();

    /**
     * Takes the next card from the shoe.
     * @return The card.
     * @throws std::logic_error if every card in the shoe has been drawn.
     */
    Card Draw();

    /**
     * Returns whether the cut card has been reached.
     * @return True once a card past the cut card has been drawn.
     */
    bool NeedsReshuffle() const;

    /**
     * Gathers every card back into the shoe and shuffles it.
     */
    void Reshuffle();

private:
    std::vector<Card> cards;
    std::size_t next = 0;
    std::mt19937 random;
};

/**
 * The cards dealt to the player or the dealer for one round, and what they
 * add up to. Aces count 11 unless that would put the hand over 21, in which
 * case they drop to 1 one at a time.
 */
class Hand final {
public:
    /**
     * Creates a hand with no cards.
     * @param from_split Whether the hand is one of the two hands made by
     * splitting a pair. A hand made by splitting can never be a blackjack.
     */
    explicit Hand(bool from_split = false);

    /**
     * Adds a card to the hand.
     * @param card The card.
     */
    void Add(Card card);

    /**
     * Returns the hand's cards, in the order they were dealt.
     * @return The cards.
     */
    const std::vector<Card> &Cards() const;

    /**
     * Finds the hand's best total, with as many aces counting 11 as can
     * without going over 21.
     * @return The total.
     */
    std::int32_t Total() const;

    /**
     * Returns whether an ace in the hand is still counting 11.
     * @return True if the hand is soft.
     */
    bool IsSoft() const;

    /**
     * Returns whether the hand is over 21.
     * @return True if the hand has bust.
     */
    bool IsBust() const;

    /**
     * Returns whether the hand is a blackjack: exactly two cards making 21,
     * in a hand that was not made by splitting.
     * @return True if the hand is a blackjack.
     */
    bool IsBlackjack() const;

private:
    std::vector<Card> cards;
    bool from_split;
};

/**
 * Who a card is dealt to.
 *
 * VALUES:
 * - Player: The player.
 * - Dealer: The dealer.
 */
enum class Seat : std::uint8_t {
    Player,
    Dealer,
};

/**
 * How a finished round turned out for the player.
 *
 * VALUES:
 * - Blackjack: The player had a blackjack and the dealer did not; pays 3:2.
 * - Push: A tie; the bet is returned.
 * - Lose: The player lost the bet.
 */
enum class Outcome : std::uint8_t {
    Blackjack,
    Push,
    Lose,
};

/**
 * A bet being placed, starting a round. Raised by the round, and handled by
 * whatever takes the bet from the player's balance.
 *
 * FIELDS:
 * - bet_cents: The bet, in cents.
 */
class BetPlacedEvent final : public svanes::EventOf<BetPlacedEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.bet_placed";

    /**
     * @param bet_cents The bet, in cents.
     */
    explicit BetPlacedEvent(std::int64_t bet_cents);

    std::int64_t bet_cents;
};

/**
 * The shoe being reshuffled before a round is dealt, because the cut card was
 * reached in an earlier round. Raised by the round.
 */
class ShoeReshuffledEvent final : public svanes::EventOf<ShoeReshuffledEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.shoe_reshuffled";
};

/**
 * A card being dealt. Raised by the round, and handled by whatever shows the
 * table.
 *
 * FIELDS:
 * - seat: Who the card is dealt to.
 * - card: The card.
 * - face_up: Whether the card is dealt face up. Only the dealer's hole card
 * is dealt face down.
 */
class CardDealtEvent final : public svanes::EventOf<CardDealtEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.card_dealt";

    /**
     * @param seat Who the card is dealt to.
     * @param card The card.
     * @param face_up Whether the card is dealt face up.
     */
    CardDealtEvent(Seat seat, Card card, bool face_up);

    Seat seat;
    Card card;
    bool face_up;
};

/**
 * The dealer checking the hole card for a blackjack, which happens when the
 * dealer's face up card is an ace or worth 10. Raised by the round.
 *
 * FIELDS:
 * - has_blackjack: Whether the dealer has a blackjack.
 */
class DealerPeekedEvent final : public svanes::EventOf<DealerPeekedEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.dealer_peeked";

    /**
     * @param has_blackjack Whether the dealer has a blackjack.
     */
    explicit DealerPeekedEvent(bool has_blackjack);

    bool has_blackjack;
};

/**
 * The dealer turning the hole card face up. Raised by the round.
 *
 * FIELDS:
 * - card: The hole card.
 */
class HoleCardRevealedEvent final
    : public svanes::EventOf<HoleCardRevealedEvent> {
public:
    static constexpr std::string_view EventName =
        "blackjack.hole_card_revealed";

    /**
     * @param card The hole card.
     */
    explicit HoleCardRevealedEvent(Card card);

    Card card;
};

/**
 * The player's turn starting, after the deal when neither side has a
 * blackjack. Raised by the round.
 */
class PlayerTurnEvent final : public svanes::EventOf<PlayerTurnEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.player_turn";
};

/**
 * A round finishing. Raised by the round, and handled by whatever gives the
 * payout to the player's balance and whatever shows the result.
 *
 * FIELDS:
 * - outcome: How the round turned out for the player.
 * - payout_cents: How much is given back to the player, in cents, including
 * the bet itself. 0 when the player loses.
 */
class RoundFinishedEvent final : public svanes::EventOf<RoundFinishedEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.round_finished";

    /**
     * @param outcome How the round turned out for the player.
     * @param payout_cents How much is given back to the player, in cents.
     */
    RoundFinishedEvent(Outcome outcome, std::int64_t payout_cents);

    Outcome outcome;
    std::int64_t payout_cents;
};

/**
 * One round of blackjack against the dealer, played with a shoe that lasts
 * across rounds. The round does not hold the player's money; it reports what
 * happens, including bets and payouts, as events.
 */
class Round final {
public:
    /**
     * Creates a round waiting for a bet, with a freshly shuffled shoe.
     * @param events Where the round reports what happens.
     */
    explicit Round(svanes::EventManager &events);

    /**
     * Places a bet and deals a new round: reshuffles first if the cut card
     * was reached, deals two cards each with the dealer's second face down,
     * and lets the dealer peek for a blackjack when showing an ace or a 10. A
     * blackjack on either side finishes the round right away; otherwise the
     * player's turn starts. The caller must already have checked that the
     * player can afford the bet.
     * @param bet The bet, in cents. Must be at least $1.
     * @throws std::invalid_argument if the bet is under $1.
     * @throws std::logic_error if a round is still being played.
     */
    void PlaceBet(std::int64_t bet);

private:
    /**
     * Where the round is.
     *
     * VALUES:
     * - Betting: Waiting for the first bet.
     * - PlayerTurn: Waiting for the player to act.
     * - Finished: Over, and waiting for the next bet.
     */
    enum class Stage : std::uint8_t {
        Betting,
        PlayerTurn,
        Finished,
    };

    /**
     * Draws a card from the shoe into a hand and reports it.
     * @param seat Who the card is dealt to.
     * @param face_up Whether the card is dealt face up.
     */
    void Deal(Seat seat, bool face_up);

    /**
     * Ends the round: turns the hole card face up and reports the result.
     * @param outcome How the round turned out for the player.
     * @param payout_cents How much is given back to the player, in cents.
     */
    void Finish(Outcome outcome, std::int64_t payout_cents);

    svanes::EventManager &events;
    Shoe shoe;
    Hand player;
    Hand dealer;
    std::int64_t bet_cents = 0;
    Stage stage = Stage::Betting;
};

} // namespace grindset::blackjack
