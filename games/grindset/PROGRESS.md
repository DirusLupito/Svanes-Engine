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
- [ ] 2.3 Gentle translate-only sway while held — **NEXT, awaiting approval**
- [ ] 2.4 Status bar (5:30 clock, balance, an emoji glyph as the early emoji check)
- [ ] 2.5 Home screen wallpaper + two clickable app icons
- [ ] 2.6 Home bar returns to home screen
- [ ] 2.7 App-open transition (rect grows from icon, content fades in) into placeholder apps
- [ ] 2.8 Remember last app when pulled out again

## Phase 3: Engine changes (flagged)
- [ ] 3.1 `ClipRect` component through the render queue, honored by shapes
- [ ] 3.2 Sprites + text labels honor clip
- [ ] 3.3 `TextLabel::wrap_width`, part of the label cache key

## Phase 4a: Blackjack rules (`blackjack.*`, no rendering)
- [ ] 4a.1 Card + 6-deck shoe with ~75% reshuffle point
- [ ] 4a.2 Hand value: soft/hard, blackjack, bust
- [ ] 4a.3 Round start: bet, deal, hole-card peek, naturals pay 3:2
- [ ] 4a.4 Hit, stand, dealer hits soft 17, settle
- [ ] 4a.5 Double (with affordability)
- [ ] 4a.6 Split: one max, any 10-values, split aces one card, 21 after split = blackjack
- [ ] 4a.7 Announcement queue (callouts, round flow, reshuffle)

## Phase 4b: Casino app UI
- [ ] 4b.1 Stream view layout: placeholder dealer behind a table
- [ ] 4b.2 Card drawing, face up / face down
- [ ] 4b.3 Chips, bet display, Clear / All in / Deal, debit through `Grindset`
- [ ] 4b.4 Hit / Stand / Double / Split buttons with enabled state
- [ ] 4b.5 Dealer turn shown, payouts credited, next round
- [ ] 4b.6 Captions from the announcement queue
- [ ] 4b.7 Leaving mid-round freezes the round

## Phase 5: Reels
- [ ] 5.1 Reel slots stacked inside the phone screen, clipped
- [ ] 5.2 Click-drag + Up/Down arrows move reels
- [ ] 5.3 Release: velocity-projected rest point, change-or-snap-back, spring settle
- [ ] 5.4 Pay rules (new reel + min watch time) + "+$0.50" popup
- [ ] 5.5 Like / dislike (visual only)
- [ ] 5.6 `text_lines` loader + POV gradient text format
- [ ] 5.7 Fake facts
- [ ] 5.8 Would you rather
- [ ] 5.9 Brainrot split screen
- [ ] 5.10 Emoji story
- [ ] 5.11 Motivational guru
- [ ] 5.12 Special reels hook

## Phase 6: Chat + dealer lines
- [ ] 6.1 Chat panel + message log
- [ ] 6.2 Key-press typing with Shift, Enter sends, phone toggle disabled while typing
- [ ] 6.3 Ambient messages
- [ ] 6.4 Random replies to the player
- [ ] 6.5 Dealer banter + chat-reply captions

## Where we left off
All work through 2.2 is committed (last: `toggle phone between pocket and hand`).
2.2b done (uncommitted). Next up is 2.3, proposed but not yet approved.

## Deviations from plan
- Emoji check moved to 2.4 (status bar glyph) so the risk is seen early.
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
