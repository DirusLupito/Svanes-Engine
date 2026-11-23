#include "blackjack.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace grindset::blackjack {

namespace {

constexpr std::int32_t kDeckCount = 6;
constexpr std::size_t kCutCardPosition = 52 * kDeckCount * 3 / 4;

constexpr std::array<Rank, 13> kRanks{
    Rank::Ace,  Rank::Two,   Rank::Three, Rank::Four, Rank::Five,
    Rank::Six,  Rank::Seven, Rank::Eight, Rank::Nine, Rank::Ten,
    Rank::Jack, Rank::Queen, Rank::King,
};

constexpr std::array<Suit, 4> kSuits{
    Suit::Clubs,
    Suit::Diamonds,
    Suit::Hearts,
    Suit::Spades,
};

/**
 * What a hand adds up to.
 *
 * FIELDS:
 * - total: The best total, with as many aces counting 11 as can without going
 * over 21.
 * - soft: Whether an ace is still counting 11 in that total.
 */
struct Count {
    std::int32_t total;
    bool soft;
};

/**
 * Adds up a hand's cards, counting each ace as 11 and then dropping aces to 1,
 * one at a time, while the total is over 21.
 *
 * @param cards The hand's cards.
 * @return The total and whether an ace still counts 11 in it.
 */
Count CountCards(const std::vector<Card> &cards) {
    std::int32_t total = 0;
    std::int32_t high_aces = 0;
    for (const Card card : cards) {
        total += CardValue(card);
        if (card.rank == Rank::Ace) {
            ++high_aces;
        }
    }
    while (total > 21 && high_aces > 0) {
        total -= 10;
        --high_aces;
    }
    return {.total = total, .soft = high_aces > 0};
}

} // namespace

std::int32_t CardValue(Card card) {
    switch (card.rank) {
    case Rank::Ace:
        return 11;
    case Rank::Two:
        return 2;
    case Rank::Three:
        return 3;
    case Rank::Four:
        return 4;
    case Rank::Five:
        return 5;
    case Rank::Six:
        return 6;
    case Rank::Seven:
        return 7;
    case Rank::Eight:
        return 8;
    case Rank::Nine:
        return 9;
    case Rank::Ten:
    case Rank::Jack:
    case Rank::Queen:
    case Rank::King:
        return 10;
    }
    throw std::invalid_argument("Card has an unknown rank.");
}

Shoe::Shoe() : random(std::random_device{}()) {
    for (std::int32_t deck = 0; deck < kDeckCount; ++deck) {
        for (const Suit suit : kSuits) {
            for (const Rank rank : kRanks) {
                cards.push_back({.rank = rank, .suit = suit});
            }
        }
    }
    Reshuffle();
}

Card Shoe::Draw() {
    if (next >= cards.size()) {
        throw std::logic_error("The shoe ran out of cards.");
    }
    const Card card = cards[next];
    ++next;
    return card;
}

bool Shoe::NeedsReshuffle() const { return next > kCutCardPosition; }

void Shoe::Reshuffle() {
    std::shuffle(cards.begin(), cards.end(), random);
    next = 0;
}

Hand::Hand(bool from_split) : from_split(from_split) {}

void Hand::Add(Card card) { cards.push_back(card); }

const std::vector<Card> &Hand::Cards() const { return cards; }

std::int32_t Hand::Total() const { return CountCards(cards).total; }

bool Hand::IsSoft() const { return CountCards(cards).soft; }

bool Hand::IsBust() const { return Total() > 21; }

bool Hand::IsBlackjack() const {
    return !from_split && cards.size() == 2 && Total() == 21;
}

} // namespace grindset::blackjack
