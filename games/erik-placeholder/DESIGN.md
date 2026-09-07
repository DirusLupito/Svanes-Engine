# Erik's game — state and plan

Side-view 2D, Terraria-boss-fight flavored, bullet-hell combat. The player is a
goose. Eventually: a sword with one-shot swing animations, a gun, and a couple
of magical weapons selected from number-key inventory slots.

`/CLAUDE.md` at the repo root overrides anything here. Most relevant: **no
comments written by anyone but the team**, no `new`/`delete`, fixed-width
integer types, braces on every block, crash loudly rather than succeed
mysteriously, and earn your complexity.

---

## 1. What exists

Iteration 1 is complete — every requirement on the assignment is met.

**Files:** `erik_game.hpp/cpp`, `goose.hpp/cpp`, `bullets.hpp/cpp`. All three
translation units are in `add_executable(svanes_game_erik ...)` in the root
`CMakeLists.txt`.

**Controls:** `A`/`D` move, `Space` jump, **left mouse** fires toward the
cursor, `Tab` toggles scale mode, `Escape` quits.

**The world:** a blue background, a static green ground strip, the goose, and a
red enemy box that sweeps side to side on a sine path dropping bullets straight
down. Getting hit — by a bullet or by touching the enemy — knocks the goose back
and grants brief invincibility.

**Build:** `just build erik`, `just run erik`. These target only this game, so
Chris's stale game can't block us.

---

## 2. Decisions worth remembering

These are also the writeup material.

### Loop order: integrate before the game updates

`src/application.cpp` used to run `game.Update` then `AdvanceKinematics`, with
nothing after. Since collision resolution lives in game code, that meant the
game corrected penetration and the integrator immediately re-penetrated — and
*that* was the state rendered. Landing at 800 px/s produced ~13px of visible
sink-and-pop.

Swapping them puts resolution between integration and render, so the penetrated
state is never drawn. Costs one frame (~16ms) of input latency. Bonus: `Update`
now sees post-integration transforms, so a camera set there matches the drawn
frame exactly — which is what makes camera-follow (§4) lag-free.

### Gravity is opt-in via a marker component

`struct Gravity {}` in `kinematic_system.hpp`, checked with `HasComponent`
inside `AdvanceKinematics`. The world gravity vector lives on `GameContext`
/`FrameContext` like `scale_mode`, defaults to `{0, 0}`, and this game sets
`{0, 2000}`.

**Opt-in, not opt-out, because of the failure mode.** With a `gravity_scale`
field on `Kinematic2D`, everything would fall by default and forgetting to zero
it on a bullet gives a silently drooping bullet. Forgetting to *add* a component
gives a bullet that flies straight — which is what you wanted anyway. Fails safe
instead of failing mysteriously.

Answers "which entities are affected by gravity": the goose has the component;
the ground and enemy don't even have `Kinematic2D`; bullets have `Kinematic2D`
but no `Gravity`.

### Positional correction, not the penalty method

`orbitalEscalation` responds to collisions by turning overlap into a repulsive
acceleration and ignoring `penetration_depth` entirely. Right for an orbital
sandbox; wrong for a platformer — a goose under gravity would sink until the
penalty force balanced its weight, then oscillate, and it never yields a clean
`grounded` boolean.

This game moves the goose out along `collision.normal * penetration_depth` and
zeroes the velocity into the surface. `grounded` falls out of `normal.y < 0`.

**Two games, one detection routine, two incompatible responses.** That's the
argument for where the engine boundary belongs: detection and broadphase in the
engine, response policy chosen by the game. The same split shows up *inside*
this game — terrain blocks, enemy contact knocks back.

### `Solid` marks what blocks movement

`Goose::ResolveCollisions` uses `ForEach<Transform, Collider2D, Solid>`, so the
query itself filters. Ground is `Solid`; the goose, enemy, and bullets are not —
otherwise walking into a bullet would shove you like a wall.

Currently declared in `goose.hpp`. Once walls exist it'll be used by more than
the goose and probably wants a better home.

### `GooseIntent` is the controller seam

`Goose::Update(frame, intent)` means the goose has no idea whether intent came
from a keyboard, an AI, or a network packet. That's the whole Pawn/Controller
split for the price of one parameter — the game commands, the goose decides how
that looks. A formal `IController` hierarchy buys nothing until a real AI exists.

### Bullets are entities, not a class

`struct Bullet { Entity owner; }` plus `Transform`, `Kinematic2D`, `SolidShape`,
`Collider2D`. Spawn and forget.

The `owner` field solves friendly fire for **both** sides with no team concept:
a bullet skips its owner and skips other bullets. The goose's and enemy's
bullets are the same code path with a different owner.

`UpdateBullets` returns `std::vector<BulletHit>` and lets `ErikGame` decide what
a hit means — the same detect/respond split as above, one level down. Bullets
don't know what a goose is. When `Health` arrives, damage drops into that loop
without touching `bullets.cpp`.

Bullets die by leaving a bounds rectangle passed in by the game, not a lifetime
timer — so range doesn't silently differ per weapon speed. **This relies on
bullets always moving**, which holds today but would break for a stationary mine
or lingering hitbox.

### Knockback is the first "locked" state

`ApplyKnockback` sets velocity and a `knockback_timer` during which `Update`
skips applying movement intent. Without that, `velocity_x = intent.move.x *
speed` would erase the knockback on the very next frame.

This is the **locked vs derived state** distinction arriving naturally: most
states are recomputed from intent every frame, but some must run to completion.
The sword swing will need exactly this shape.

`invincible_timer` (0.8s) is separate from `knockback_timer` (0.25s) — they mean
different things. **`ApplyKnockback` gates on invincibility itself**, so
`ErikGame` just reports hits and the goose decides whether they land. That's why
the enemy-contact path needs no cooldown of its own.

### The enemy is deliberately not a class

It has no health, animation, facing, inventory, or brain — its path is a pure
function of time, so nothing ever commands it. A class exists to give you a
handle for commanding something.

More importantly, writing one now would mean designing the shared `Character`
abstraction from one real character and one fake one.

**Trigger to change this:** its state (`elapsed_seconds`, `enemy_fire_cooldown`)
lives on `ErikGame`, which works for exactly one enemy. A second enemy, or real
behavior like chasing and attack patterns, makes it a genuine character — and
the second data point that tells you what `Character` should contain.

---

## 3. Engine changes made

1. **Loop order swap** in `src/application.cpp` (§2).
2. **Global directional gravity** — `Gravity` marker in `kinematic_system.hpp`,
   `gravity` on both contexts, applied in `AdvanceKinematics`. Purely additive:
   no existing game needed a line.
3. **`ScreenToWorldPoint`** in `render/render_system.hpp`. `Camera2D::ScreenToWorld`
   only inverts camera and zoom, never `scale`/`offset`, so mouse aiming was
   silently wrong under `ScaleMode::Proportional` — the exact mode the assignment
   requires demoing. Now built on a teammate's `ComputeRenderLayout` so there's
   one source of truth for the layout math.

---

## 4. Next up

### Camera follows the goose

Set `camera.x = goose.x - (output_width * 0.5F) / zoom` (and y likewise) in
`ErikGame::Update` **after** `goose.Update`, so it uses the post-collision
position. Thanks to the loop order change this is lag-free.

Knock-ons:

- The background is a fixed 1920x1080 rect and would scroll away. Either make it
  much larger or pin it to the camera each frame, the way `orbitalEscalation`
  does with its `view` rectangle.
- The ground needs to be much wider.
- `kBulletBounds` should become true world bounds rather than a screen-sized box.
- **World border**: border rectangles tagged `Solid` are blocking for free, since
  the goose already resolves against anything `Solid`.

### Let the goose fly

Needs a `GooseState::Flying`, `intent.move.y` driving vertical velocity, and
gravity off while flying.

**This is the case that may earn a field on `Gravity`.** With the bare marker,
toggling flight means `RemoveComponent`/`AddComponent` on every transition —
component churn where `scale = 0.0F` would be one assignment. We deliberately
left the field out until something needed it; this is that something. Decide
when implementing, not before.

---

## 5. Known gaps

Engine-side, each blocking real work here and generally useful:

1. **One-shot animations.** `SpriteAnimation` always loops and never reports
   completion. Wanted independently by the idle fidget, the attack lock, and
   knockback — three features, same missing feature.
2. **Horizontal flip on `Sprite`.** No flip field, so a goose walking left still
   faces right. Blocks displaying facing at all.
3. **A real collision system.** Nothing iterates the world or builds candidate
   pairs; every game writes its own loop. Bullet collision is currently
   O(bullets x entities).
4. **Early-exit `ForEach`.** `Registry::ForEach` discards the lambda's return
   value, so there's no way to stop iterating once an answer is found. Wanted by
   bullet collision now and by any broadphase later.
5. **Sprite tint or alpha**, so invincibility frames can flash. Currently
   invincibility is functional but invisible.
6. **Texture path cache.** Every `LoadTexture` re-decodes and re-uploads.

Game-side, deferred until there's evidence for the design:

- `Character` base class — needs a real second character (§2).
- `Health` and damage — the hooks are in place in the `BulletHit` loop.
- Inventory, weapons, the sword — need an actual item to exist first.
- `GooseAssets` — only when a second goose appears.
- Anything network-shaped. The preparation that matters is keeping shared data
  in components rather than class members, so serialization stays generic.
