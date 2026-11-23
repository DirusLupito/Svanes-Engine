# Grindset — Progress

One checkbox per commit (~100 lines each). See `DESIGN.md` for the spec.

## Phase 1: Skeleton + road
- [x] 1.1 Build target, justfile, `main.cpp`, empty `Grindset` opening a 1920x1080 window
- [x] 1.2 Sunrise sky gradient + sun
- [x] 1.3 Road in perspective (static)
- [x] 1.4 Scrolling lane dashes
- [x] 1.5 Telephone poles (left) + unlit street lamps (right)
- [x] 1.6 Sagging wires between telephone poles
- [x] 1.7a Refactor: `Road` records each entity's resting y and can shift them all by one vertical offset (no visual change)
- [x] 1.7b Faint camera bob (two slow sines, ~±6px) driving that offset

## Phase 2: Phone
- [x] 2.1 Phone frame + screen rect in held position
- [x] 2.1b First-person car interior around the road view
- [x] 2.2 Space/Tab pocket ↔ held toggle with eased slide (starts in pocket)
- [x] 2.2b Fonts + assets setup (pulled from 2.4), hint text on screen to take out the phone
- [x] 2.3 Gentle translate-only sway while held
- [x] 2.4 Status bar (5:30am clock, balance)
- [x] 2.5 Home screen wallpaper + two clickable app icons
- [x] 2.6 App-open transition (rect grows from icon, content fades in) into placeholder apps
- [x] 2.7 Home bar: swipe up from the bottom to close an app
- [x] 2.8 Remember last app when pulled out again (already worked; app state survives pocketing)

## Phase 3: Engine changes (flagged)
- [x] 3.1 `ClipRect` component through the render queue, honored by shapes
- [x] 3.2 Sprites, tile maps, radial gradients + text labels honor clip
- [x] 3.3 `TextLabel::wrap_width`, part of the label cache key

## Phase 4a: Blackjack rules (`blackjack.*`, no rendering)
- [x] 4a.1 Card + 6-deck shoe with ~75% reshuffle point
- [x] 4a.2 Hand value: soft/hard, blackjack, bust
- [x] 4a.3 Round start: bet, deal, hole-card peek, naturals pay 3:2 (reported as events)
- [x] 4a.4 Hit, stand, dealer hits soft 17, settle
- [x] 4a.5 Double (with affordability)
- [x] 4a.6 Split: one max, any 10-values, split aces one card, 21 after split pays 1:1
- [x] 4a.7 Remaining round events (bust, dealer stood, hand doubled/split/settled)

## Phase 4b: Casino app UI
- [x] 4b.1 Stream view layout: placeholder dealer behind a table
- [x] 4b.2 Card drawing, face up / face down
- [x] 4b.3 Chips, bet display, Clear / All in / Deal, debit through `Grindset`
- [x] 4b.4 Hit / Stand / Double / Split buttons with enabled state
- [x] 4b.5 Dealer turn shown, payouts credited, next round
- [x] 4b.6 Captions from round events
- [x] 4b.7 Leaving mid-round freezes the round (dealer steps and caption timer pause)

## Phase 5: Reels
- [x] 5.1 Reel slots stacked inside the phone screen, clipped
- [x] 5.2 Click-drag + Up/Down arrows move reels
- [x] 5.3 Release: velocity-projected rest point, change-or-snap-back, spring settle
- [x] 5.4 Pay rules (3s on a new reel pays) + "+$0.50" popup
- [x] 5.5 Like / dislike (visual only)
- [x] 5.6 Reel formats (merged 5.6-5.11 at Erik's request): `text_lines` loader, generator, view; POV, fake facts, would you rather, brainrot, emoji story (Twemoji COLRv0 font), guru
- [ ] 5.12 Special reels hook

## Phase 6: Chat + dealer lines
- [ ] 6.1 Chat panel + message log
- [ ] 6.2 Key-press typing with Shift, Enter sends, phone toggle disabled while typing
- [ ] 6.3 Ambient messages
- [ ] 6.4 Random replies to the player
- [ ] 6.5 Dealer banter + chat-reply captions

## Where we left off
Reels through like/dislike committed. 5.6 (all reel formats) done (uncommitted), not yet run; emoji rendering untested. Next up is 5.12 (special reels hook).

## Deviations from plan
- 5.6-5.11 merged into one step (Erik). Added `reel_view.*` (draws any format from a fixed pool of bands, shapes, labels). Emoji via Mozilla Twemoji (COLRv0, works with FreeType built without PNG); Noto Color Emoji would need PNG support.
- Emoji check dropped from 2.4 (Erik); revisit when reels need emoji (5.10), with the PNG or no-emoji fallback still open.
- `svanes_game_package` + `GRINDSET_ASSETS_DIR` added in 2.2b with the first asset (font); packaging needs an `assets/` folder. Font is goose's `consola.ttf` (28pt), loaded once in `Grindset`.
- 1.5 split into poles/lamps (1.5) and wires (1.6); camera bob moved to 1.7.
- Poles and lamps use even spacing, not random gaps (wires need regular poles).
- Road is pseudo-3D: positions are (lateral, height, depth) in camera heights,
  projected by `ProjectPoint` in `road.cpp`; camera sits in the right lane.
- Camera bob can't use the engine camera: text labels ignore it, so the phone
  text would drift from the phone frame. Bob is applied inside `Road` instead.
- Phone is 422x880 at x=1400, bottom edge at y=1040 (not 60% coverage; first tried 480x1000, too tall). All phone parts share one
  Transform at the phone center with geometry relative to it, so moving the
  phone is one position. Phone z starts at 100, leaving room for the car interior.
- 2.1b car interior added by Erik, right after the phone frame.
- Car interior (`car_interior.*`): roof, left pillar only (thick, wider at the bottom, leaning strongly out toward the top),
  hood sliver, plain dashboard, centered steering wheel top (rim from short
  pieces), big non-reflecting mirror (460x140 at x=1250) overlapping the phone's left edge, partly
  behind it. All parts rest at Transform y 0 (absolute geometry), so bobbing is setting y; it
  moves 40% of `Road::CurrentBob()`.
- Phone starts in the pocket with on-screen text telling you to take it out
  (Erik). The text needs fonts, so the font/assets setup moves from 2.4 to 2.2b.
- Status bar lives in `Phone`; `Grindset` owns `balance_cents` ($100 start) and
  passes it to `Phone::Update` each frame, so the phone never holds the money.
  Clock reads "5:30am" (Erik). Status font is the same Consolas at 20pt.
- Home screen lives in `Phone` for now. Wallpaper fade = bands between the
  rounded ends; the rounded top is the screen shape itself and the rounded bottom
  is a "cap" shape in the bottom color, so no clipping is needed. Icons are plain
  rounded squares (not real logos); a click darkens the icon until release, and
  only counts while the phone is fully held.
- 2.6/2.7 swapped: app opening comes before the home bar, so the home bar has
  something to close. Icons sit on a 3-column grid (80px icons) after the first
  layout put Instagram right of center.
- App opening: click = press and release on the same icon. One "cover" shape
  in the app color is rebuilt each frame, growing (0.3s, smoothstep) from the
  icon to the screen with its corner radius; then the placeholder title fades
  in (0.25s). Icons are hidden by alpha 0 (shapes have no visible flag).
- Going home is a swipe up from a 70px strip at the bottom of an open app (no
  click, no button; Erik). The app follows the drag. On release: close if
  dragged + speed x 0.5s >= 75% of screen height, else spring back; speed is
  measured over the last 60ms of samples. A critically damped spring settles
  it, starting at the release speed.
- Clip design: `RenderQueue::SetClip` sets a clip that every later command
  carries (no new args on the Draw functions). Each queued command is stored
  with its clip; the executor overlaps it with the pass clip, only calls SDL
  when it changes, and skips commands whose overlap is empty. `ClipRect` is in
  world coordinates and goes through the camera like shapes.
- 21 after a split is a normal 21 (1:1), not blackjack: Erik chose normal casino
  rules when asked, replacing the earlier house rule.
- Rounds report through engine events (triggered immediately) instead of an
  announcement queue (Erik); 4a.7 shrinks to whatever events are still missing.
  `Grindset` will debit/credit from bet-placed / round-finished events.
- 4a.4-4a.7 built in one go (Erik, no visuals to check). The round keeps a list
  of player hands from the start so splitting needs no rework. Money events:
  every bet (first, double, split) is a `BetPlacedEvent`; `RoundFinishedEvent`
  carries the total payout, with per-hand `HandSettledEvent`s for captions.
  Auto-stand on 21. A hand is dealt its second card after a split when it
  starts being played; split aces get one card each, then the dealer plays.
- Apps draw their own content: `Phone` dropped the placeholder title and exposes
  `AppShown(app)` (fade) and `AppArea()` (current cover bounds, used as the
  app's `ClipRect`), plus a z band from `Phone::kAppZ`. Instagram shows only
  its color until reels exist.
- 4b.2 + 4b.3 combined so cards have a deal to show (Erik). `CasinoApp` owns the
  `Round` and draws cards from its events; `Grindset` moves money on
  `BetPlacedEvent` / `RoundFinishedEvent` and throws if a bet exceeds the
  balance. `FormatDollars` moved to `phone.hpp` so both can use it. Suits are
  UTF-8 byte escapes so MSVC prints them without `/utf-8`. The bet stays set
  after a round, ready to deal again.
- Engine issue (not fixed, Erik to decide): `ZOrder` docs promise same-z draws in
  creation order, but `Registry` stores entities in an `std::unordered_map`, so
  same-z order can change. Showed up as a fully black card (border over face).
  Grindset now never relies on same-z order for overlapping shapes.
- Dealer pacing: `Round` has a `DealerTurn` stage; `DealerStep()` draws one card
  or settles. The casino steps it every 2.5s (tried 0.6s, too fast; 4s, too slow), only while the app is fully
  shown, so payouts land after the last dealer card is seen.
- Round result is a big banner (28pt, dark translucent panel sized from the
  text length) between the dealer and player rows, with the net across all
  hands. Per-hand words under the hands only appear after a split.
- Captions: every line said in one frame is joined into one caption (several
  events fire per frame, e.g. the deal), wrapped to the stream width with
  `wrap_width`, held 5s then faded over 0.3s. Lines live in code until Phase 6.
- Row totals sit right of each card row (face up cards only, so the hole card
  is not given away; red when bust). After a split the right hand's total can
  run past the screen edge when soft or after several hits (clipped).
