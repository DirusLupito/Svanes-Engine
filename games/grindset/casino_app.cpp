#include "casino_app.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace grindset {

namespace {

constexpr float kStreamTop = 44.0F;
constexpr float kStreamHeight = 320.0F;

constexpr float kDealerHeadRadius = 34.0F;
constexpr float kDealerHeadFromTop = 120.0F;
constexpr float kDealerBodyWidth = 170.0F;
constexpr float kDealerBodyHeight = 120.0F;
constexpr float kDealerBodyFromTop = 215.0F;
constexpr float kDealerCollarWidth = 30.0F;
constexpr float kDealerCollarHeight = 40.0F;

constexpr float kTableHeight = 70.0F;
constexpr float kRailHeight = 10.0F;

constexpr float kBadgeWidth = 58.0F;
constexpr float kBadgeHeight = 26.0F;
constexpr float kBadgeInset = 16.0F;

constexpr float kDealerCardsFromTop = 426.0F;
constexpr float kPlayerCardsFromTop = 540.0F;
constexpr float kBetFromTop = 628.0F;
constexpr float kCardWidth = 56.0F;
constexpr float kCardHeight = 80.0F;
constexpr float kCardSpread = 40.0F;
constexpr float kCardBorder = 2.0F;
constexpr svanes::Vector2D kCardLabelInset{6.0F, 5.0F};
constexpr float kHandMarkerWidth = 60.0F;
constexpr float kHandMarkerHeight = 5.0F;
constexpr float kHandMarkerGap = 12.0F;

constexpr std::int32_t kStreamZ = Phone::kAppZ;
constexpr std::int32_t kDealerBodyZ = Phone::kAppZ + 1;
constexpr std::int32_t kDealerCollarZ = Phone::kAppZ + 2;
constexpr std::int32_t kDealerHeadZ = Phone::kAppZ + 3;
constexpr std::int32_t kTableZ = Phone::kAppZ + 4;
constexpr std::int32_t kRailZ = Phone::kAppZ + 5;
constexpr std::int32_t kBadgeZ = Phone::kAppZ + 6;
constexpr std::int32_t kControlZ = Phone::kAppZ + 7;
constexpr std::int32_t kCardZ = Phone::kAppZ + 8;

constexpr svanes::Color kStreamColor{.red = 14, .green = 14, .blue = 20};
constexpr svanes::Color kDealerHeadColor{.red = 206, .green = 170, .blue = 140};
constexpr svanes::Color kDealerBodyColor{.red = 30, .green = 30, .blue = 38};
constexpr svanes::Color kDealerCollarColor{
    .red = 230, .green = 230, .blue = 235};
constexpr svanes::Color kTableColor{.red = 20, .green = 92, .blue = 56};
constexpr svanes::Color kRailColor{.red = 86, .green = 52, .blue = 30};
constexpr svanes::Color kBadgeColor{.red = 214, .green = 40, .blue = 52};
constexpr svanes::Color kWhite{.red = 255, .green = 255, .blue = 255};
constexpr svanes::Color kCardFaceColor{.red = 246, .green = 246, .blue = 242};
constexpr svanes::Color kCardBackColor{.red = 44, .green = 70, .blue = 160};
constexpr svanes::Color kCardBorderColor{.red = 20, .green = 20, .blue = 24};
constexpr svanes::Color kBlackSuitColor{.red = 20, .green = 20, .blue = 24};
constexpr svanes::Color kRedSuitColor{.red = 200, .green = 30, .blue = 40};
constexpr float kDisabledShade = 0.45F;
constexpr float kDealerStepSeconds = 0.6F;
constexpr float kResultGap = 4.0F;
constexpr svanes::Color kWinColor{.red = 240, .green = 200, .blue = 80};
constexpr svanes::Color kLoseColor{.red = 236, .green = 120, .blue = 120};
constexpr svanes::Color kHandMarkerColor{.red = 240, .green = 200, .blue = 80};

/**
 * How one of the app's controls looks and where it sits.
 *
 * FIELDS:
 * - text: The control's text.
 * - x: The control's center, sideways from the middle of the screen.
 * - from_top: The control's center, down from the top of the screen.
 * - width: The control's width.
 * - height: The control's height.
 * - chip: Whether the control is a round chip rather than a button.
 * - color: The control's color.
 * - text_color: The color of the control's text.
 * - chip_cents: How much the control adds to the bet, or 0 if it is not a
 * chip.
 * - during_round: Whether the control is shown while a round is being played,
 * rather than between rounds.
 */
struct ControlSpec {
    const char *text;
    float x;
    float from_top;
    float width;
    float height;
    bool chip;
    svanes::Color color;
    svanes::Color text_color;
    std::int64_t chip_cents;
    bool during_round;
};

constexpr float kChipsFromTop = 672.0F;
constexpr float kActionsFromTop = 742.0F;
constexpr float kChipSize = 56.0F;
constexpr float kActionWidth = 110.0F;
constexpr float kActionHeight = 44.0F;

constexpr float kPlayWidth = 160.0F;
constexpr float kPlayHeight = 48.0F;
constexpr float kPlayX = 86.0F;

constexpr std::size_t kClear = 4;
constexpr std::size_t kAllIn = 5;
constexpr std::size_t kDeal = 6;
constexpr std::size_t kHit = 7;
constexpr std::size_t kStand = 8;
constexpr std::size_t kDouble = 9;
constexpr std::size_t kSplit = 10;

constexpr std::array<ControlSpec, 11> kControls{{
    {.text = "$1",
     .x = -135.0F,
     .from_top = kChipsFromTop,
     .width = kChipSize,
     .height = kChipSize,
     .chip = true,
     .color = {.red = 222, .green = 222, .blue = 222},
     .text_color = {.red = 30, .green = 30, .blue = 30},
     .chip_cents = 100},
    {.text = "$5",
     .x = -45.0F,
     .from_top = kChipsFromTop,
     .width = kChipSize,
     .height = kChipSize,
     .chip = true,
     .color = {.red = 196, .green = 40, .blue = 44},
     .text_color = kWhite,
     .chip_cents = 500},
    {.text = "$25",
     .x = 45.0F,
     .from_top = kChipsFromTop,
     .width = kChipSize,
     .height = kChipSize,
     .chip = true,
     .color = {.red = 30, .green = 140, .blue = 70},
     .text_color = kWhite,
     .chip_cents = 2'500},
    {.text = "$100",
     .x = 135.0F,
     .from_top = kChipsFromTop,
     .width = kChipSize,
     .height = kChipSize,
     .chip = true,
     .color = {.red = 34, .green = 34, .blue = 38},
     .text_color = kWhite,
     .chip_cents = 10'000},
    {.text = "Clear",
     .x = -125.0F,
     .from_top = kActionsFromTop,
     .width = kActionWidth,
     .height = kActionHeight,
     .chip = false,
     .color = {.red = 70, .green = 70, .blue = 82},
     .text_color = kWhite,
     .chip_cents = 0},
    {.text = "All in",
     .x = 0.0F,
     .from_top = kActionsFromTop,
     .width = kActionWidth,
     .height = kActionHeight,
     .chip = false,
     .color = {.red = 160, .green = 116, .blue = 28},
     .text_color = kWhite,
     .chip_cents = 0},
    {.text = "Deal",
     .x = 125.0F,
     .from_top = kActionsFromTop,
     .width = kActionWidth,
     .height = kActionHeight,
     .chip = false,
     .color = {.red = 30, .green = 110, .blue = 200},
     .text_color = kWhite,
     .chip_cents = 0},
    {.text = "Hit",
     .x = -kPlayX,
     .from_top = kChipsFromTop,
     .width = kPlayWidth,
     .height = kPlayHeight,
     .chip = false,
     .color = {.red = 30, .green = 140, .blue = 70},
     .text_color = kWhite,
     .chip_cents = 0,
     .during_round = true},
    {.text = "Stand",
     .x = kPlayX,
     .from_top = kChipsFromTop,
     .width = kPlayWidth,
     .height = kPlayHeight,
     .chip = false,
     .color = {.red = 196, .green = 40, .blue = 44},
     .text_color = kWhite,
     .chip_cents = 0,
     .during_round = true},
    {.text = "Double",
     .x = -kPlayX,
     .from_top = kActionsFromTop,
     .width = kPlayWidth,
     .height = kPlayHeight,
     .chip = false,
     .color = {.red = 160, .green = 116, .blue = 28},
     .text_color = kWhite,
     .chip_cents = 0,
     .during_round = true},
    {.text = "Split",
     .x = kPlayX,
     .from_top = kActionsFromTop,
     .width = kPlayWidth,
     .height = kPlayHeight,
     .chip = false,
     .color = {.red = 30, .green = 110, .blue = 200},
     .text_color = kWhite,
     .chip_cents = 0,
     .during_round = true},
}};

/**
 * Fades an alpha by how much of the app is showing.
 *
 * @param alpha The alpha when the app is fully shown.
 * @param shown How much of the app is showing, from 0 to 1.
 * @return The faded alpha.
 */
std::uint8_t Faded(std::uint8_t alpha, float shown) {
    return static_cast<std::uint8_t>(static_cast<float>(alpha) * shown);
}

/**
 * Darkens a color, keeping its alpha.
 *
 * @param color The color.
 * @param amount How much of the color's brightness to keep, from 0 to 1.
 * @return The darkened color.
 */
svanes::Color Shade(svanes::Color color, float amount) {
    const auto shade = [amount](std::uint8_t channel) {
        return static_cast<std::uint8_t>(static_cast<float>(channel) * amount);
    };
    return {.red = shade(color.red),
            .green = shade(color.green),
            .blue = shade(color.blue),
            .alpha = color.alpha};
}

/**
 * Writes a card's rank the way it is printed in the card's corner.
 *
 * @param rank The rank.
 * @return The rank as text, such as "A", "7", or "K".
 */
std::string RankText(blackjack::Rank rank) {
    switch (rank) {
    case blackjack::Rank::Ace:
        return "A";
    case blackjack::Rank::Jack:
        return "J";
    case blackjack::Rank::Queen:
        return "Q";
    case blackjack::Rank::King:
        return "K";
    default:
        return std::to_string(blackjack::CardValue({.rank = rank}));
    }
}

/**
 * Writes a card's suit as its symbol, in UTF-8.
 *
 * @param suit The suit.
 * @return The suit's symbol.
 */
std::string SuitText(blackjack::Suit suit) {
    switch (suit) {
    case blackjack::Suit::Clubs:
        return "\xE2\x99\xA3";
    case blackjack::Suit::Diamonds:
        return "\xE2\x99\xA6";
    case blackjack::Suit::Hearts:
        return "\xE2\x99\xA5";
    case blackjack::Suit::Spades:
        return "\xE2\x99\xA0";
    }
    throw std::invalid_argument("Card has an unknown suit.");
}

/**
 * Returns whether a suit is printed in red.
 *
 * @param suit The suit.
 * @return True for diamonds and hearts.
 */
bool IsRed(blackjack::Suit suit) {
    return suit == blackjack::Suit::Diamonds || suit == blackjack::Suit::Hearts;
}

/**
 * Finds where a control's center is on the phone's screen.
 *
 * @param spec The control.
 * @param screen Where the phone's screen is.
 * @return The control's center, in world coordinates.
 */
svanes::Vector2D ControlCenter(const ControlSpec &spec,
                               svanes::Rectangle2D screen) {
    return {screen.x + spec.x, screen.y - screen.height * 0.5F + spec.from_top};
}

/**
 * Moves, colors, fades, and clips one of the app's text labels.
 *
 * @param world The world holding it.
 * @param entity The label's entity.
 * @param position Where the label goes, in world coordinates.
 * @param color The label's color when the app is fully shown.
 * @param area The area the app currently covers.
 * @param shown How much of the app is showing, from 0 to 1.
 */
void PlaceLabel(svanes::Registry &world, svanes::Entity entity,
                svanes::Vector2D position, svanes::Color color,
                svanes::Rectangle2D area, float shown) {
    svanes::TextLabel &label = world.GetComponent<svanes::TextLabel>(entity);
    label.position = position;
    label.color = color;
    label.color.alpha = Faded(color.alpha, shown);
    label.visible = shown > 0.0F;
    world.GetComponent<svanes::ClipRect>(entity).area = area;
}

/**
 * Moves, colors, fades, and clips one of the app's shapes.
 *
 * @param world The world holding it.
 * @param entity The shape's entity.
 * @param position Where the shape's Transform goes, in world coordinates.
 * @param color The shape's color when the app is fully shown.
 * @param area The area the app currently covers.
 * @param shown How much of the app is showing, from 0 to 1.
 */
void PlaceShape(svanes::Registry &world, svanes::Entity entity,
                svanes::Vector2D position, svanes::Color color,
                svanes::Rectangle2D area, float shown) {
    svanes::Transform &transform =
        world.GetComponent<svanes::Transform>(entity);
    transform.x = position.x;
    transform.y = position.y;
    svanes::SolidShape &shape = world.GetComponent<svanes::SolidShape>(entity);
    shape.color = color;
    shape.color.alpha = Faded(color.alpha, shown);
    world.GetComponent<svanes::ClipRect>(entity).area = area;
}

/**
 * Creates one of the app's shapes, hidden and clipped until it is placed.
 *
 * @param world The world to create it in.
 * @param geometry The shape, relative to its Transform.
 * @param z_order The z order to draw it at.
 * @return The shape's entity.
 */
svanes::Entity CreateShape(svanes::Registry &world, svanes::Geometry2D geometry,
                           std::int32_t z_order) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{});
    world.AddComponent<svanes::SolidShape>(
        entity, svanes::SolidShape{.color = {.alpha = 0},
                                   .geometry = std::move(geometry)});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    world.AddComponent<svanes::ClipRect>(entity, svanes::ClipRect{});
    return entity;
}

/**
 * Creates one of the app's text labels, hidden and clipped until it is placed.
 *
 * @param world The world to create it in.
 * @param text The label's text.
 * @param font The font to write it in.
 * @param alignment Which part of the text sits at its position.
 * @param z_order The z order to draw it at.
 * @return The label's entity.
 */
svanes::Entity CreateLabel(svanes::Registry &world, std::string text,
                           svanes::FontHandle font,
                           svanes::TextAlignment alignment,
                           std::int32_t z_order) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(
        entity, svanes::TextLabel{.text = std::move(text),
                                  .font = font,
                                  .alignment = alignment,
                                  .visible = false});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    world.AddComponent<svanes::ClipRect>(entity, svanes::ClipRect{});
    return entity;
}

} // namespace

CasinoApp::Result CasinoApp::ResultText(blackjack::Outcome outcome,
                                        std::int64_t payout_cents,
                                        std::int64_t bet_cents, bool bust) {
    switch (outcome) {
    case blackjack::Outcome::Blackjack:
        return {.text = "Blackjack +" + FormatDollars(payout_cents - bet_cents),
                .color = kWinColor};
    case blackjack::Outcome::Win:
        return {.text = "Win +" + FormatDollars(payout_cents - bet_cents),
                .color = kWinColor};
    case blackjack::Outcome::Push:
        return {.text = "Push", .color = kWhite};
    case blackjack::Outcome::Lose:
        return {.text = bust ? "Bust" : "Lose", .color = kLoseColor};
    }
    throw std::invalid_argument("Hand has an unknown outcome.");
}

void CasinoApp::Initialize(svanes::Registry &world,
                           svanes::EventManager &events,
                           svanes::FontHandle label_font, const Phone &phone) {
    font = label_font;
    const svanes::Rectangle2D screen = phone.ScreenRectangle();
    const float top = -screen.height * 0.5F + kStreamTop;
    const float stream_bottom = top + kStreamHeight;
    const auto add = [&](svanes::Color color, svanes::Geometry2D geometry,
                         std::int32_t z_order) {
        parts.push_back(
            {.entity = CreateShape(world, std::move(geometry), z_order),
             .color = color});
    };

    add(kStreamColor,
        svanes::Rectangle2D{.y = top + kStreamHeight * 0.5F,
                            .width = screen.width,
                            .height = kStreamHeight},
        kStreamZ);
    add(kDealerBodyColor,
        svanes::Rectangle2D{.y = top + kDealerBodyFromTop,
                            .width = kDealerBodyWidth,
                            .height = kDealerBodyHeight},
        kDealerBodyZ);
    add(kDealerCollarColor,
        svanes::Rectangle2D{.y = top + kDealerBodyFromTop -
                                 (kDealerBodyHeight - kDealerCollarHeight) *
                                     0.5F,
                            .width = kDealerCollarWidth,
                            .height = kDealerCollarHeight},
        kDealerCollarZ);
    add(kDealerHeadColor,
        svanes::Circle2D{.y = top + kDealerHeadFromTop,
                         .radius = kDealerHeadRadius},
        kDealerHeadZ);
    add(kTableColor,
        svanes::Rectangle2D{.y = stream_bottom - kTableHeight * 0.5F,
                            .width = screen.width,
                            .height = kTableHeight},
        kTableZ);
    add(kRailColor,
        svanes::Rectangle2D{.y = stream_bottom - kTableHeight +
                                 kRailHeight * 0.5F,
                            .width = screen.width,
                            .height = kRailHeight},
        kRailZ);
    add(kBadgeColor,
        svanes::Rectangle2D{.x = -screen.width * 0.5F + kBadgeInset +
                                 kBadgeWidth * 0.5F,
                            .y = top + kBadgeInset + kBadgeHeight * 0.5F,
                            .width = kBadgeWidth,
                            .height = kBadgeHeight},
        kBadgeZ);

    live_label = CreateLabel(world, "LIVE", font, svanes::TextAlignment::Center,
                             kBadgeZ);
    bet_label =
        CreateLabel(world, "", font, svanes::TextAlignment::Center, kControlZ);
    for (std::size_t index = 0; index < kControls.size(); ++index) {
        const ControlSpec &spec = kControls[index];
        svanes::Geometry2D geometry =
            spec.chip ? svanes::Geometry2D{svanes::Circle2D{
                            .radius = spec.width * 0.5F}}
                      : svanes::Geometry2D{svanes::Rectangle2D{
                            .width = spec.width, .height = spec.height}};
        buttons[index] = {
            .shape = CreateShape(world, std::move(geometry), kControlZ),
            .label = CreateLabel(world, spec.text, font,
                                 svanes::TextAlignment::Center, kControlZ)};
    }

    for (svanes::Entity &label : result_labels) {
        label = CreateLabel(world, "", font, svanes::TextAlignment::TopCenter,
                            kControlZ);
    }
    hand_marker = CreateShape(world,
                              svanes::Rectangle2D{.width = kHandMarkerWidth,
                                                  .height = kHandMarkerHeight},
                              kControlZ);

    round.emplace(events);
    events.Subscribe(std::make_unique<
                     svanes::EventHandlerWrapper<blackjack::BetPlacedEvent>>(
        [this, &world](const blackjack::BetPlacedEvent &event) {
            if (in_play) {
                return;
            }
            ClearCards(world);
            in_play = true;
            active_hand = 0;
            hand_bets = {event.bet_cents};
            busted = {false};
            results.clear();
        }));
    events.Subscribe(std::make_unique<
                     svanes::EventHandlerWrapper<blackjack::HandDoubledEvent>>(
        [this](const blackjack::HandDoubledEvent &event) {
            hand_bets.at(event.hand_index) *= 2;
        }));
    events.Subscribe(
        std::make_unique<svanes::EventHandlerWrapper<blackjack::HandBustEvent>>(
            [this](const blackjack::HandBustEvent &event) {
                if (event.seat == blackjack::Seat::Player) {
                    busted.at(event.hand_index) = true;
                }
            }));
    events.Subscribe(std::make_unique<
                     svanes::EventHandlerWrapper<blackjack::HandSettledEvent>>(
        [this](const blackjack::HandSettledEvent &event) {
            results.push_back(ResultText(event.outcome, event.payout_cents,
                                         hand_bets.at(event.hand_index),
                                         busted.at(event.hand_index)));
        }));
    events.Subscribe(std::make_unique<
                     svanes::EventHandlerWrapper<blackjack::HandSplitEvent>>(
        [this](const blackjack::HandSplitEvent &) {
            const CardView second = player_hands.at(0).at(1);
            player_hands.at(0).pop_back();
            player_hands.push_back({second});
            hand_bets.push_back(hand_bets.at(0));
            busted.push_back(false);
        }));
    events.Subscribe(std::make_unique<
                     svanes::EventHandlerWrapper<blackjack::PlayerTurnEvent>>(
        [this](const blackjack::PlayerTurnEvent &event) {
            active_hand = event.hand_index;
        }));
    events.Subscribe(std::make_unique<
                     svanes::EventHandlerWrapper<blackjack::CardDealtEvent>>(
        [this, &world](const blackjack::CardDealtEvent &event) {
            CardView card = CreateCard(world, event.card, event.face_up);
            if (event.seat == blackjack::Seat::Dealer) {
                dealer_cards.push_back(card);
                return;
            }
            if (player_hands.size() <= event.hand_index) {
                player_hands.resize(event.hand_index + 1);
            }
            player_hands[event.hand_index].push_back(card);
        }));
    events.Subscribe(
        std::make_unique<
            svanes::EventHandlerWrapper<blackjack::HoleCardRevealedEvent>>(
            [this](const blackjack::HoleCardRevealedEvent &) {
                dealer_cards.at(1).face_up = true;
            }));
    events.Subscribe(
        std::make_unique<
            svanes::EventHandlerWrapper<blackjack::RoundFinishedEvent>>(
            [this](const blackjack::RoundFinishedEvent &) {
                in_play = false;
            }));
}

CasinoApp::CardView CasinoApp::CreateCard(svanes::Registry &world,
                                          blackjack::Card card,
                                          bool face_up) const {
    const svanes::Entity border = CreateShape(
        world,
        svanes::Rectangle2D{.width = kCardWidth + kCardBorder * 2.0F,
                            .height = kCardHeight + kCardBorder * 2.0F},
        kCardZ);
    const svanes::Entity body = CreateShape(
        world, svanes::Rectangle2D{.width = kCardWidth, .height = kCardHeight},
        kCardZ);
    const svanes::Entity label =
        CreateLabel(world, RankText(card.rank) + SuitText(card.suit), font,
                    svanes::TextAlignment::TopLeft, kCardZ);
    return {.border = border,
            .body = body,
            .label = label,
            .card = card,
            .face_up = face_up};
}

void CasinoApp::ClearCards(svanes::Registry &world) {
    for (const CardView &card : dealer_cards) {
        world.DestroyEntity(card.border);
        world.DestroyEntity(card.body);
        world.DestroyEntity(card.label);
    }
    for (const std::vector<CardView> &hand : player_hands) {
        for (const CardView &card : hand) {
            world.DestroyEntity(card.border);
            world.DestroyEntity(card.body);
            world.DestroyEntity(card.label);
        }
    }
    dealer_cards.clear();
    player_hands.clear();
}

bool CasinoApp::IsVisible(std::size_t index) const {
    if (round->IsDealerTurn()) {
        return false;
    }
    return kControls[index].during_round == in_play;
}

bool CasinoApp::IsEnabled(std::size_t index, std::int64_t balance_cents) const {
    if (!IsVisible(index)) {
        return false;
    }
    if (index == kHit || index == kStand) {
        return true;
    }
    if (index == kDouble) {
        return round->CanDouble(balance_cents);
    }
    if (index == kSplit) {
        return round->CanSplit(balance_cents);
    }
    const ControlSpec &spec = kControls[index];
    if (spec.chip_cents > 0) {
        return bet_cents + spec.chip_cents <= balance_cents;
    }
    if (index == kClear) {
        return bet_cents > 0;
    }
    if (index == kAllIn) {
        return balance_cents >= 100 && bet_cents != balance_cents;
    }
    return bet_cents >= 100 && bet_cents <= balance_cents;
}

void CasinoApp::Activate(std::size_t index, std::int64_t balance_cents) {
    const ControlSpec &spec = kControls[index];
    if (spec.chip_cents > 0) {
        bet_cents += spec.chip_cents;
    } else if (index == kClear) {
        bet_cents = 0;
    } else if (index == kAllIn) {
        bet_cents = balance_cents;
    } else if (index == kDeal) {
        round->PlaceBet(bet_cents);
    } else if (index == kHit) {
        round->Hit();
    } else if (index == kStand) {
        round->Stand();
    } else if (index == kDouble) {
        round->Double();
    } else if (index == kSplit) {
        round->Split();
    }
}

void CasinoApp::TrackClick(const svanes::InputManager &input,
                           svanes::Rectangle2D screen,
                           std::int64_t balance_cents) {
    if (input.WasMouseButtonReleased(svanes::MouseButton::Left)) {
        const std::optional<std::size_t> released =
            ControlUnderMouse(input, screen);
        if (pressed_button.has_value() && released == pressed_button &&
            IsEnabled(*pressed_button, balance_cents)) {
            Activate(*pressed_button, balance_cents);
        }
        pressed_button.reset();
        return;
    }
    if (input.WasMouseButtonPressed(svanes::MouseButton::Left)) {
        const std::optional<std::size_t> pressed =
            ControlUnderMouse(input, screen);
        if (pressed.has_value() && IsEnabled(*pressed, balance_cents)) {
            pressed_button = pressed;
        }
    }
}

std::optional<std::size_t>
CasinoApp::ControlUnderMouse(const svanes::InputManager &input,
                             svanes::Rectangle2D screen) const {
    const svanes::Vector2D mouse = input.MousePosition();
    for (std::size_t index = 0; index < kControls.size(); ++index) {
        const ControlSpec &spec = kControls[index];
        const svanes::Vector2D center = ControlCenter(spec, screen);
        if (IsVisible(index) &&
            std::abs(mouse.x - center.x) <= spec.width * 0.5F &&
            std::abs(mouse.y - center.y) <= spec.height * 0.5F) {
            return index;
        }
    }
    return std::nullopt;
}

void CasinoApp::PlaceCards(svanes::Registry &world,
                           const std::vector<CardView> &cards,
                           svanes::Vector2D center, svanes::Rectangle2D area,
                           float shown) const {
    const float first =
        -kCardSpread * (static_cast<float>(cards.size()) - 1.0F) * 0.5F;
    for (std::size_t index = 0; index < cards.size(); ++index) {
        const CardView &card = cards[index];
        const svanes::Vector2D position{
            center.x + first + kCardSpread * static_cast<float>(index),
            center.y};
        const std::int32_t z_order =
            kCardZ + 2 * static_cast<std::int32_t>(index);
        world.GetComponent<svanes::ZOrder>(card.border).value = z_order;
        world.GetComponent<svanes::ZOrder>(card.body).value = z_order + 1;
        PlaceShape(world, card.border, position, kCardBorderColor, area, shown);
        PlaceShape(world, card.body, position,
                   card.face_up ? kCardFaceColor : kCardBackColor, area, shown);
        PlaceLabel(world, card.label,
                   {position.x - kCardWidth * 0.5F + kCardLabelInset.x,
                    position.y - kCardHeight * 0.5F + kCardLabelInset.y},
                   IsRed(card.card.suit) ? kRedSuitColor : kBlackSuitColor,
                   area, card.face_up ? shown : 0.0F);
    }
}

void CasinoApp::Update(svanes::Registry &world,
                       const svanes::InputManager &input, const Phone &phone,
                       float seconds, std::int64_t balance_cents) {
    const svanes::Rectangle2D screen = phone.ScreenRectangle();
    const svanes::Rectangle2D area = phone.AppArea();
    const float shown = phone.AppShown(Phone::App::Casino);
    const float top = screen.y - screen.height * 0.5F;

    if (round->IsDealerTurn() && shown == 1.0F) {
        dealer_seconds += seconds;
        if (dealer_seconds >= kDealerStepSeconds) {
            dealer_seconds = 0.0F;
            round->DealerStep();
        }
    } else {
        dealer_seconds = 0.0F;
    }

    if (shown == 1.0F && phone.IsFullyHeld()) {
        TrackClick(input, screen, balance_cents);
    } else {
        pressed_button.reset();
    }

    for (const Part &part : parts) {
        PlaceShape(world, part.entity, {screen.x, screen.y}, part.color, area,
                   shown);
    }
    PlaceLabel(
        world, live_label,
        {screen.x - screen.width * 0.5F + kBadgeInset + kBadgeWidth * 0.5F,
         top + kStreamTop + kBadgeInset + kBadgeHeight * 0.5F},
        kWhite, area, shown);

    world.GetComponent<svanes::TextLabel>(bet_label).text =
        "Bet " + FormatDollars(bet_cents);
    PlaceLabel(world, bet_label, {screen.x, top + kBetFromTop}, kWhite, area,
               shown);

    for (std::size_t index = 0; index < kControls.size(); ++index) {
        const ControlSpec &spec = kControls[index];
        const bool enabled = IsEnabled(index, balance_cents);
        svanes::Color color =
            enabled ? spec.color : Shade(spec.color, kDisabledShade);
        if (pressed_button == index) {
            color = Shade(color, 0.8F);
        }
        const svanes::Vector2D center = ControlCenter(spec, screen);
        const float control_shown = IsVisible(index) ? shown : 0.0F;
        PlaceShape(world, buttons[index].shape, center, color, area,
                   control_shown);
        PlaceLabel(world, buttons[index].label, center,
                   enabled ? spec.text_color
                           : Shade(spec.text_color, kDisabledShade),
                   area, control_shown);
    }

    PlaceCards(world, dealer_cards, {screen.x, top + kDealerCardsFromTop}, area,
               shown);
    const float hand_spacing =
        player_hands.size() > 1
            ? screen.width / static_cast<float>(player_hands.size())
            : 0.0F;
    const float first_hand_x =
        screen.x -
        hand_spacing * (static_cast<float>(player_hands.size()) - 1.0F) * 0.5F;
    for (std::size_t index = 0; index < player_hands.size(); ++index) {
        PlaceCards(world, player_hands[index],
                   {first_hand_x + hand_spacing * static_cast<float>(index),
                    top + kPlayerCardsFromTop},
                   area, shown);
    }
    const bool mark_hand = in_play && player_hands.size() > 1;
    PlaceShape(
        world, hand_marker,
        {first_hand_x + hand_spacing * static_cast<float>(active_hand),
         top + kPlayerCardsFromTop + kCardHeight * 0.5F + kHandMarkerGap},
        kHandMarkerColor, area, mark_hand ? shown : 0.0F);

    for (std::size_t index = 0; index < result_labels.size(); ++index) {
        const bool has_result = !in_play && index < results.size();
        if (has_result) {
            world.GetComponent<svanes::TextLabel>(result_labels[index]).text =
                results[index].text;
        }
        PlaceLabel(
            world, result_labels[index],
            {first_hand_x + hand_spacing * static_cast<float>(index),
             top + kPlayerCardsFromTop + kCardHeight * 0.5F + kResultGap},
            has_result ? results[index].color : kWhite, area,
            has_result ? shown : 0.0F);
    }
}

} // namespace grindset
