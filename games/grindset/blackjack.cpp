#include "blackjack.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace grindset::blackjack {

namespace {

constexpr std::int32_t kDeckCount = 6;
constexpr std::size_t kCutCardPosition = 52 * kDeckCount * 3 / 4;
constexpr std::int64_t kMinimumBetCents = 100;

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

BetPlacedEvent::BetPlacedEvent(std::int64_t bet_cents) : bet_cents(bet_cents) {}

CardDealtEvent::CardDealtEvent(Seat seat, std::size_t hand_index, Card card,
                               bool face_up)
    : seat(seat), hand_index(hand_index), card(card), face_up(face_up) {}

DealerPeekedEvent::DealerPeekedEvent(bool has_blackjack)
    : has_blackjack(has_blackjack) {}

HoleCardRevealedEvent::HoleCardRevealedEvent(Card card) : card(card) {}

PlayerTurnEvent::PlayerTurnEvent(std::size_t hand_index)
    : hand_index(hand_index) {}

HandDoubledEvent::HandDoubledEvent(std::size_t hand_index)
    : hand_index(hand_index) {}

HandBustEvent::HandBustEvent(Seat seat, std::size_t hand_index)
    : seat(seat), hand_index(hand_index) {}

DealerStoodEvent::DealerStoodEvent(std::int32_t total) : total(total) {}

HandSettledEvent::HandSettledEvent(std::size_t hand_index, Outcome outcome,
                                   std::int64_t payout_cents)
    : hand_index(hand_index), outcome(outcome), payout_cents(payout_cents) {}

RoundFinishedEvent::RoundFinishedEvent(std::int64_t payout_cents)
    : payout_cents(payout_cents) {}

namespace {

/**
 * Finds how much a settled hand gives back to the player. A blackjack pays
 * 3:2, rounded down to the cent, and a win pays 1:1. Both amounts include the
 * bet itself.
 *
 * @param outcome How the hand turned out.
 * @param bet_cents The money bet on the hand, in cents.
 * @return The payout, in cents.
 */
std::int64_t Payout(Outcome outcome, std::int64_t bet_cents) {
    switch (outcome) {
    case Outcome::Blackjack:
        return bet_cents + bet_cents * 3 / 2;
    case Outcome::Win:
        return bet_cents * 2;
    case Outcome::Push:
        return bet_cents;
    case Outcome::Lose:
        return 0;
    }
    throw std::invalid_argument("Hand has an unknown outcome.");
}

} // namespace

Round::Round(svanes::EventManager &events) : events(events) {}

void Round::PlaceBet(std::int64_t bet) {
    if (stage == Stage::PlayerTurn || stage == Stage::DealerTurn) {
        throw std::logic_error(
            "A bet cannot be placed while a round is being played.");
    }
    if (bet < kMinimumBetCents) {
        throw std::invalid_argument("A bet must be at least $1, but was " +
                                    std::to_string(bet) + " cents.");
    }

    hands = {PlayerHand{.hand = Hand{}, .bet_cents = bet}};
    active = 0;
    dealer = Hand{};
    hole_card_shown = false;
    events.TriggerEvent(BetPlacedEvent{bet});
    if (shoe.NeedsReshuffle()) {
        shoe.Reshuffle();
        events.TriggerEvent(ShoeReshuffledEvent{});
    }

    DealPlayer(0);
    DealDealer(true);
    DealPlayer(0);
    DealDealer(false);

    if (CardValue(dealer.Cards().front()) >= 10) {
        events.TriggerEvent(DealerPeekedEvent{dealer.IsBlackjack()});
    }
    if (dealer.IsBlackjack()) {
        Finish(
            {hands.front().hand.IsBlackjack() ? Outcome::Push : Outcome::Lose});
        return;
    }
    if (hands.front().hand.IsBlackjack()) {
        Finish({Outcome::Blackjack});
        return;
    }
    stage = Stage::PlayerTurn;
    StartHand();
}

void Round::Hit() {
    RequirePlayerTurn("hit");
    DealPlayer(active);
    const Hand &hand = hands[active].hand;
    if (hand.IsBust()) {
        events.TriggerEvent(HandBustEvent{Seat::Player, active});
        EndHand();
    } else if (hand.Total() == 21) {
        EndHand();
    }
}

void Round::Stand() {
    RequirePlayerTurn("stand");
    EndHand();
}

bool Round::CanDouble(std::int64_t balance_cents) const {
    return stage == Stage::PlayerTurn &&
           hands[active].hand.Cards().size() == 2 &&
           balance_cents >= hands[active].bet_cents;
}

void Round::Double() {
    RequirePlayerTurn("double down");
    if (hands[active].hand.Cards().size() != 2) {
        throw std::logic_error(
            "Only a hand with its first two cards can be doubled.");
    }
    events.TriggerEvent(BetPlacedEvent{hands[active].bet_cents});
    hands[active].bet_cents *= 2;
    events.TriggerEvent(HandDoubledEvent{active});
    DealPlayer(active);
    if (hands[active].hand.IsBust()) {
        events.TriggerEvent(HandBustEvent{Seat::Player, active});
    }
    EndHand();
}

bool Round::CanSplit(std::int64_t balance_cents) const {
    return SplitAllowed() && balance_cents >= hands.front().bet_cents;
}

void Round::Split() {
    if (!SplitAllowed()) {
        throw std::logic_error("This hand cannot be split.");
    }
    const Card first = hands.front().hand.Cards()[0];
    const Card second = hands.front().hand.Cards()[1];
    const std::int64_t bet = hands.front().bet_cents;
    events.TriggerEvent(BetPlacedEvent{bet});

    hands = {PlayerHand{.hand = Hand{true}, .bet_cents = bet},
             PlayerHand{.hand = Hand{true}, .bet_cents = bet}};
    hands[0].hand.Add(first);
    hands[1].hand.Add(second);
    active = 0;
    events.TriggerEvent(HandSplitEvent{});

    if (first.rank == Rank::Ace) {
        DealPlayer(0);
        DealPlayer(1);
        active = hands.size();
        StartDealerTurn();
        return;
    }
    DealPlayer(0);
    StartHand();
}

void Round::RequirePlayerTurn(std::string_view action) const {
    if (stage != Stage::PlayerTurn) {
        throw std::logic_error("The player cannot " + std::string{action} +
                               " outside of their turn.");
    }
}

bool Round::SplitAllowed() const {
    if (stage != Stage::PlayerTurn || hands.size() != 1) {
        return false;
    }
    const std::vector<Card> &cards = hands.front().hand.Cards();
    return cards.size() == 2 && CardValue(cards[0]) == CardValue(cards[1]);
}

void Round::DealPlayer(std::size_t hand_index) {
    const Card card = shoe.Draw();
    hands[hand_index].hand.Add(card);
    events.TriggerEvent(CardDealtEvent{Seat::Player, hand_index, card, true});
}

void Round::DealDealer(bool face_up) {
    const Card card = shoe.Draw();
    dealer.Add(card);
    events.TriggerEvent(CardDealtEvent{Seat::Dealer, 0, card, face_up});
}

void Round::EndHand() {
    ++active;
    if (active < hands.size()) {
        DealPlayer(active);
        StartHand();
        return;
    }
    StartDealerTurn();
}

void Round::StartHand() {
    if (hands[active].hand.Total() == 21) {
        EndHand();
        return;
    }
    events.TriggerEvent(PlayerTurnEvent{active});
}

void Round::RevealHoleCard() {
    if (hole_card_shown) {
        return;
    }
    hole_card_shown = true;
    events.TriggerEvent(HoleCardRevealedEvent{dealer.Cards()[1]});
}

void Round::StartDealerTurn() {
    RevealHoleCard();
    const bool any_standing =
        std::any_of(hands.begin(), hands.end(),
                    [](const PlayerHand &hand) { return !hand.hand.IsBust(); });
    if (!any_standing) {
        Settle();
        return;
    }
    stage = Stage::DealerTurn;
}

bool Round::IsDealerTurn() const { return stage == Stage::DealerTurn; }

void Round::DealerStep() {
    if (stage != Stage::DealerTurn) {
        throw std::logic_error(
            "The dealer cannot take a step outside of their turn.");
    }
    if (dealer.Total() < 17 || (dealer.Total() == 17 && dealer.IsSoft())) {
        DealDealer(true);
        return;
    }
    if (dealer.IsBust()) {
        events.TriggerEvent(HandBustEvent{Seat::Dealer, 0});
    } else {
        events.TriggerEvent(DealerStoodEvent{dealer.Total()});
    }
    Settle();
}

void Round::Settle() {
    std::vector<Outcome> outcomes;
    for (const PlayerHand &hand : hands) {
        outcomes.push_back(Judge(hand));
    }
    Finish(outcomes);
}

Outcome Round::Judge(const PlayerHand &player_hand) const {
    if (player_hand.hand.IsBust()) {
        return Outcome::Lose;
    }
    if (dealer.IsBust() || player_hand.hand.Total() > dealer.Total()) {
        return Outcome::Win;
    }
    if (player_hand.hand.Total() == dealer.Total()) {
        return Outcome::Push;
    }
    return Outcome::Lose;
}

void Round::Finish(const std::vector<Outcome> &outcomes) {
    RevealHoleCard();
    std::int64_t total = 0;
    for (std::size_t index = 0; index < hands.size(); ++index) {
        const std::int64_t payout =
            Payout(outcomes[index], hands[index].bet_cents);
        events.TriggerEvent(HandSettledEvent{index, outcomes[index], payout});
        total += payout;
    }
    stage = Stage::Finished;
    events.TriggerEvent(RoundFinishedEvent{total});
}

} // namespace grindset::blackjack
