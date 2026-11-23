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
 * How one of the player's hands turned out.
 *
 * VALUES:
 * - Blackjack: The hand was a blackjack and the dealer's was not; pays 3:2.
 * - Win: The hand beat the dealer, or the dealer bust; pays 1:1.
 * - Push: A tie; the bet is returned.
 * - Lose: The hand lost its bet.
 */
enum class Outcome : std::uint8_t {
    Blackjack,
    Win,
    Push,
    Lose,
};

/**
 * Money being bet: the first bet of a round, or the extra bet for doubling
 * down or splitting. Raised by the round, and handled by whatever takes the
 * money from the player's balance.
 *
 * FIELDS:
 * - bet_cents: The money bet, in cents.
 */
class BetPlacedEvent final : public svanes::EventOf<BetPlacedEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.bet_placed";

    /**
     * @param bet_cents The money bet, in cents.
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
 * - hand_index: Which of the player's hands the card goes to, from 0. Always
 * 0 for the dealer.
 * - card: The card.
 * - face_up: Whether the card is dealt face up. Only the dealer's hole card
 * is dealt face down.
 */
class CardDealtEvent final : public svanes::EventOf<CardDealtEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.card_dealt";

    /**
     * @param seat Who the card is dealt to.
     * @param hand_index Which of the player's hands the card goes to.
     * @param card The card.
     * @param face_up Whether the card is dealt face up.
     */
    CardDealtEvent(Seat seat, std::size_t hand_index, Card card, bool face_up);

    Seat seat;
    std::size_t hand_index;
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
 * One of the player's hands becoming the one to act on. Raised by the round.
 *
 * FIELDS:
 * - hand_index: Which of the player's hands is being played, from 0.
 */
class PlayerTurnEvent final : public svanes::EventOf<PlayerTurnEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.player_turn";

    /**
     * @param hand_index Which of the player's hands is being played.
     */
    explicit PlayerTurnEvent(std::size_t hand_index);

    std::size_t hand_index;
};

/**
 * The player doubling down on a hand: its bet doubles, and it gets exactly one
 * more card. Raised by the round.
 *
 * FIELDS:
 * - hand_index: Which of the player's hands was doubled, from 0.
 */
class HandDoubledEvent final : public svanes::EventOf<HandDoubledEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.hand_doubled";

    /**
     * @param hand_index Which of the player's hands was doubled.
     */
    explicit HandDoubledEvent(std::size_t hand_index);

    std::size_t hand_index;
};

/**
 * The player splitting a pair into two hands, each with one of the pair and
 * its own bet: hand 0 keeps the first card of the pair and hand 1 the second.
 * Raised by the round, before the new hands are dealt to.
 */
class HandSplitEvent final : public svanes::EventOf<HandSplitEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.hand_split";
};

/**
 * A hand going over 21. Raised by the round.
 *
 * FIELDS:
 * - seat: Whose hand bust.
 * - hand_index: Which of the player's hands bust, from 0. Always 0 for the
 * dealer.
 */
class HandBustEvent final : public svanes::EventOf<HandBustEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.hand_bust";

    /**
     * @param seat Whose hand bust.
     * @param hand_index Which of the player's hands bust.
     */
    HandBustEvent(Seat seat, std::size_t hand_index);

    Seat seat;
    std::size_t hand_index;
};

/**
 * The dealer stopping drawing without going over 21. Raised by the round.
 *
 * FIELDS:
 * - total: The dealer's final total.
 */
class DealerStoodEvent final : public svanes::EventOf<DealerStoodEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.dealer_stood";

    /**
     * @param total The dealer's final total.
     */
    explicit DealerStoodEvent(std::int32_t total);

    std::int32_t total;
};

/**
 * One of the player's hands being settled against the dealer. Raised by the
 * round, once for each hand, before the round finishes.
 *
 * FIELDS:
 * - hand_index: Which of the player's hands was settled, from 0.
 * - outcome: How the hand turned out.
 * - payout_cents: How much the hand gives back to the player, in cents,
 * including its bet. 0 when the hand loses.
 */
class HandSettledEvent final : public svanes::EventOf<HandSettledEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.hand_settled";

    /**
     * @param hand_index Which of the player's hands was settled.
     * @param outcome How the hand turned out.
     * @param payout_cents How much the hand gives back, in cents.
     */
    HandSettledEvent(std::size_t hand_index, Outcome outcome,
                     std::int64_t payout_cents);

    std::size_t hand_index;
    Outcome outcome;
    std::int64_t payout_cents;
};

/**
 * A round finishing, after every hand is settled. Raised by the round, and
 * handled by whatever gives the payout to the player's balance.
 *
 * FIELDS:
 * - payout_cents: How much all of the player's hands give back together, in
 * cents, including their bets.
 */
class RoundFinishedEvent final : public svanes::EventOf<RoundFinishedEvent> {
public:
    static constexpr std::string_view EventName = "blackjack.round_finished";

    /**
     * @param payout_cents How much all of the player's hands give back.
     */
    explicit RoundFinishedEvent(std::int64_t payout_cents);

    std::int64_t payout_cents;
};

/**
 * One round of blackjack against the dealer, played with a shoe that lasts
 * across rounds. The dealer hits soft 17 and peeks for a blackjack when
 * showing an ace or a 10. The player may double down on any first two cards,
 * including after a split, and may split one pair per round, including any two
 * cards worth 10; split aces get one card each. The dealer's turn is taken one
 * step at a time with DealerStep, so it can be shown at a readable pace. The
 * round does not hold the player's money; it reports what happens, including
 * every bet and payout, as events.
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

    /**
     * Deals the hand being played another card. If that takes it over 21 or
     * to exactly 21, the hand is done and play moves on.
     * @throws std::logic_error if it is not the player's turn.
     */
    void Hit();

    /**
     * Ends the hand being played. Once every hand is done, the dealer's turn
     * starts.
     * @throws std::logic_error if it is not the player's turn.
     */
    void Stand();

    /**
     * Returns whether the dealer is taking their turn, waiting for DealerStep.
     * @return True during the dealer's turn.
     */
    bool IsDealerTurn() const;

    /**
     * Takes the next step of the dealer's turn: draws one card while the
     * dealer is under 17 or on a soft 17, and otherwise stands or reports the
     * bust and settles the round. Calling this one step at a time lets the
     * dealer's turn be shown at a readable pace.
     * @throws std::logic_error if it is not the dealer's turn.
     */
    void DealerStep();

    /**
     * Returns whether the hand being played can be doubled down on: it has
     * only its first two cards, and the player can afford to match its bet.
     * @param balance_cents The player's balance, in cents.
     * @return True if the hand can be doubled.
     */
    bool CanDouble(std::int64_t balance_cents) const;

    /**
     * Doubles the bet on the hand being played, deals it exactly one more
     * card, and ends it. The caller must already have checked CanDouble.
     * @throws std::logic_error if it is not the player's turn or the hand has
     * more than two cards.
     */
    void Double();

    /**
     * Returns whether the player can split: it is the first hand of the round,
     * it holds only two cards of equal value, and the player can afford a
     * second bet.
     * @param balance_cents The player's balance, in cents.
     * @return True if the hand can be split.
     */
    bool CanSplit(std::int64_t balance_cents) const;

    /**
     * Splits the pair into two hands with a bet each. Each hand is dealt its
     * second card when it starts being played. Split aces are each dealt one
     * card, and then the dealer's turn starts. The caller must already have
     * checked CanSplit.
     * @throws std::logic_error if the hand cannot be split, ignoring the
     * player's balance.
     */
    void Split();

private:
    /**
     * Where the round is.
     *
     * VALUES:
     * - Betting: Waiting for the first bet.
     * - PlayerTurn: Waiting for the player to act on a hand.
     * - DealerTurn: Waiting for the dealer to take each step of their turn.
     * - Finished: Over, and waiting for the next bet.
     */
    enum class Stage : std::uint8_t {
        Betting,
        PlayerTurn,
        DealerTurn,
        Finished,
    };

    /**
     * One of the player's hands and the money bet on it.
     *
     * FIELDS:
     * - hand: The hand's cards.
     * - bet_cents: The money bet on the hand, in cents.
     */
    struct PlayerHand {
        Hand hand;
        std::int64_t bet_cents;
    };

    /**
     * Throws unless the player is acting on a hand.
     * @param action The action being attempted, for the error message.
     * @throws std::logic_error if it is not the player's turn.
     */
    void RequirePlayerTurn(std::string_view action) const;

    /**
     * Returns whether the hand being played holds a pair that can be split,
     * ignoring the player's balance.
     * @return True if the rules allow splitting.
     */
    bool SplitAllowed() const;

    /**
     * Draws a card from the shoe into one of the player's hands and reports
     * it.
     * @param hand_index Which of the player's hands gets the card.
     */
    void DealPlayer(std::size_t hand_index);

    /**
     * Draws a card from the shoe into the dealer's hand and reports it.
     * @param face_up Whether the card is dealt face up.
     */
    void DealDealer(bool face_up);

    /**
     * Ends the hand being played and moves on: the next hand is dealt its
     * second card and starts, or once every hand is done, the dealer's
     * turn starts.
     */
    void EndHand();

    /**
     * Starts the hand being played, or ends it right away if it is already at
     * 21.
     */
    void StartHand();

    /**
     * Turns the dealer's hole card face up, unless it already is.
     */
    void RevealHoleCard();

    /**
     * Starts the dealer's turn by turning the hole card face up, or settles
     * the round right away if every player hand bust.
     */
    void StartDealerTurn();

    /**
     * Settles every player hand against the dealer's final hand and finishes
     * the round.
     */
    void Settle();

    /**
     * Finds how a player hand that was played out does against the dealer.
     * @param player_hand The hand.
     * @return How the hand turned out.
     */
    Outcome Judge(const PlayerHand &player_hand) const;

    /**
     * Settles every player hand with the outcome given for each, then finishes
     * the round.
     * @param outcomes How each of the player's hands turned out, in order.
     */
    void Finish(const std::vector<Outcome> &outcomes);

    svanes::EventManager &events;
    Shoe shoe;
    std::vector<PlayerHand> hands;
    std::size_t active = 0;
    Hand dealer;
    bool hole_card_shown = false;
    Stage stage = Stage::Betting;
};

} // namespace grindset::blackjack
