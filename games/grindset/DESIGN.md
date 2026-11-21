# Grindset — Design & Technical Handoff

Single-player, purely 2D. You drive an endless road at sunrise (5:30am, frozen)
and play on your phone: blackjack at a fake live-dealer table, and Instagram
style reels that pay $0.50 per new reel when you're broke. Turn-based argument:
blackjack (bet → player turn → dealer turn).

## Features

### Road (view 1)
- Fixed 1920x1080 window. Sunrise sky gradient + sun, road in perspective.
- Slight motion: scrolling lane dashes, occasional roadside poles, faint camera bob.

### Phone
- Space/Tab toggles pocket ↔ held (ignored while typing in chat).
- Held in the right side of the view, large (422x880, bottom edge at y=1040,
  x=1400), fully on screen since it is all players interact with; slides up/down with easing;
  gentle translate-only sway while held (text can't rotate).
- Status bar: clock fixed at 5:30, balance.
- Home bar at the bottom returns to the home screen.
- Remembers the last app when pulled out again.
- App open: rectangle expands from the icon to the screen, then content fades in.

### Home screen (view 2)
- Two app icons: Casino, Instagram. Plain wallpaper.

### Blackjack (view 3)
- Rules: 6-deck shoe, reshuffle at ~75% (announced); dealer hits soft 17;
  US hole-card peek on Ace/10; natural pays 3:2; double on any first two cards;
  one split max; any two 10-value cards may split; double after split;
  split aces get one card each; **21 after split counts as blackjack (3:2)**
  (nonstandard, intentional); double/split disabled when unaffordable.
- Betting: $1 minimum, cents allowed. Chips $1/$5/$25/$100 + All in + Clear, Deal.
- Player-paced. Leaving mid-round freezes the round.
- Stream view: placeholder dealer behind a table (image swap later), cards shown.
- Dealer captions: card callouts, round flow, random banter, chat replies.
- Chat panel: free typing (built from key presses + Shift, US layout), Enter sends.
  Ambient messages trickle in; occasional random replies to the player.

### Reels (view 4)
- Swipe via click-and-drag or Up/Down arrows.
- iOS-style feel: on release, project the resting point from release velocity;
  change reel if the projection passes halfway, else snap back; spring settle.
- Pays $0.50 only for a never-seen reel, and only after a minimum watch time on
  the previous reel. "+$0.50" popup. Like/dislike are visual only.
- Generated formats (animated where it fits): POV gradient text, fake facts,
  would-you-rather split, brainrot split screen (fake scrolling gameplay strip),
  emoji story (bouncing color emoji), motivational guru silhouette.
- Hook for hand-made special reels, mixed in at some rate; content supplied later.

### Content
- Editable text files in `assets/text/`: word lists per reel format, chat lines,
  chat replies, dealer banter, dealer reply lines. Loaded at startup.
- Fonts in `assets/fonts/` (UI font + color emoji font).

### Not in scope (for now)
Audio, visible other players, endgame/goal, recommendation algorithm.

## Engine changes (flagged)
1. **Clip rectangle** — optional `ClipRect` component (screen-space rectangle)
   in `render_system.hpp`; honored by shapes, sprites, and text labels. Render
   queue commands carry an optional clip; the executor applies it per command.
2. **Text wrap width** — `TextLabel::wrap_width` (0 = only explicit newlines),
   passed to `TTF_RenderText_Blended_Wrapped` and part of the label cache key.

## Risk to check early
- Color emoji through SDL_ttf/FreeType. Fallback: small emoji PNGs, or just no emojis at all TBD.

## Files (`games/grindset/`)
| File | Responsibility |
|---|---|
| `main.cpp` | Builds `Application`, runs `Grindset`. |
| `grindset.hpp/.cpp` | `IGame`. Owns everything below, the balance (cents, `int64_t`), view state, toggle key. |
| `road.hpp/.cpp` | Sky/sun/road entities and their motion. |
| `phone.hpp/.cpp` | Phone frame, slide/sway, status bar, home bar, home screen, app open transition, screen rect for apps. |
| `blackjack.hpp/.cpp` | Pure rules, no rendering: `Card`, `Shoe`, `Hand`, round state machine, payouts, events for captions. |
| `casino_app.hpp/.cpp` | Renders the table/dealer/cards/captions/chips/buttons; drives `blackjack` from input. |
| `chat.hpp/.cpp` | Chat log, key-based text entry, ambient + reply timers. |
| `reels_app.hpp/.cpp` | Swipe physics, reel slot entities, pay rules, like/dislike, popup. |
| `reel_generator.hpp/.cpp` | Formats + word lists → reel description; special reels. |
| `text_lines.hpp/.cpp` | Load a line-per-entry text file; random pick. |
| `CMakeLists.txt`, `justfile` | Target `svanes_game_grindset`, `GRINDSET_ASSETS_DIR`, `svanes_game_package`. |

## Key interfaces (sketch)
- Each view/app: `Initialize(GameContext&)`, `Update(const FrameContext&, ...)`,
  plus `SetVisible(bool)` so entities hide when not shown.
- `Phone` exposes the current screen `Rectangle2D` so apps lay out inside it
  and set their `ClipRect` from it.
- `blackjack::Round`: `PlaceBet(cents)`, `Hit()`, `Stand()`, `Double()`,
  `Split()`, query `CanDouble/CanSplit(balance)`, state + hands for display,
  and a queue of announcements (`"Player has 14"`, `"Dealer busts"`, ...) that
  `casino_app` turns into captions.
- Money flows through `Grindset`: apps request debit/credit, `Grindset` owns the
  number.

## Build order
1. Skeleton game + road view.
2. Phone: pocket toggle, slide, sway, status bar, home screen, app transitions.
3. Engine: clip rect + wrap width.
4. Blackjack rules (`blackjack.*`), then the casino app UI.
5. Reels: swipe physics + pay rules with one format; then remaining formats + emoji.
6. Chat + dealer lines (last).
