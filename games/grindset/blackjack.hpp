#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
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

} // namespace grindset::blackjack
