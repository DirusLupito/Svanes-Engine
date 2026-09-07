# Erik's game — design notes

Working notes for the goose (the player character) and how it sits on top of
the Svanes engine. Written so this design conversation can be resumed on
another machine with no other context.

Read `/CLAUDE.md` at the repo root first — its coding rules override anything
here. Most relevant: **no comments in code**, no `new`/`delete`, fixed-width
integer types, braces on every block, crash loudly rather than succeed
mysteriously, and earn your complexity before reaching for a language feature.

---

## 1. Current state of this game

### Files

- `erik_game.hpp` / `erik_game.cpp` — the `ErikGame : svanes::IGame`
  implementation.
- `main.cpp` — entry point.
- `assets/` — `goose.png` (29×27, single still frame), `goose_walk.png`
  (116×27, four 29×27 frames left to right),
  `darkworld_spawn_swirlingorb_idle.png` (512×128, four 128×128 frames).

### What was just fixed

The engine rewrite removed `svanes::SolidColor` and stopped using a bare
`Rectangle2D` component for size. Shapes now live inside the render
components:

- `SolidColor` + separate `Rectangle2D` component → one
  `SolidShape{ .color, .geometry }`.
- `Sprite` gained its own `.geometry` field, so sprites no longer need a
  separate `Rectangle2D` component either.

`erik_game.cpp` has been updated for both. **This has not been compiled yet** —
`just build` was never run after the edit, so treat it as unverified.

### What is broken right now

`erik_game.cpp:11-30` contains a half-written `class Goose` that is not valid
C++ (statements at class scope, references to a `context` that isn't in scope).
It is a sketch, not code. **Delete it** and replace it with the real
`goose.hpp` / `goose.cpp` described below.

`erik_game.hpp` currently has only `svanes::Entity orb{}` and
`bool should_quit`. It has no goose member yet.

---

## 2. Engine facts this design depends on

All verified by reading the engine source. Re-check before relying on them —
the engine is under active development by the team.

### The frame loop (`src/application.cpp`)

```
game.Initialize(game_context)          // once, line 58

// per frame:
game.Update(frame_context)             // line 93
AdvanceKinematics(world, delta)        // line 94
AdvanceSpriteAnimations(world, delta)  // line 96
game.ShouldQuit()                      // line 98
SubmitShapes(...)                      // line 115
SubmitSprites(...)                     // line 116
```

The ordering matters a lot: game code runs **before** physics integration and
**before** animation advancement, and there is no step at all after
integration. See the one-frame collision problem in §7.

### Contexts (`include/svanes/game.hpp`)

- `GameContext{ world, assets, camera, output_width, output_height, scale_mode& }`
  — passed to `Initialize` only. This is the only place `TextureManager` is
  reachable.
- `FrameContext{ world, input, delta_seconds, output_width, output_height, camera, scale_mode& }`
  — passed to `Update` as a `const&`. The reference members are still writable
  through the const struct, which is why `frame.scale_mode = ...` compiles.

`IGame` is two-phase: the game object is constructed with no context, and only
later receives one in `Initialize`. That constraint propagates to the goose
(§4).

### Registry (`include/svanes/registry.hpp`)

`std::unordered_map<Entity, std::unordered_map<std::type_index, std::any>>`.

- `AddComponent<T>(entity, args...)` — **replaces** any existing component of
  that type wholesale. Re-adding is how you reset a component.
- `RemoveComponent<T>(entity)` — no-op if absent.
- `HasComponent<T>(entity)`, `GetComponent<T>(entity)`.
- `ForEach<A, B...>(func)` — calls `func(entity, A&, B&...)` for entities that
  have all of them.

`GetComponent` on a missing component goes through `.at()` and
`std::any_cast`, so it throws `std::out_of_range` or `std::bad_any_cast` with
no useful context. Guard against that with an explicit check and a real
message.

**Do not cache component references across frames.** `AddComponent` reassigns
the `std::any` slot, which can relocate the contained object and leave a stored
reference dangling. Look components up fresh each frame.

### Components in play

| Component | Header | Fields |
|---|---|---|
| `Transform` | `geometry.hpp` | `x`, `y`, `rotation` |
| `Sprite` | `render/render_system.hpp` | `texture`, `source` (`std::optional<Rectangle2D>`), `geometry` (`Rectangle2D`) |
| `SolidShape` | `render/render_system.hpp` | `color`, `geometry` (`Geometry2D` variant) |
| `ZOrder` | `render/render_system.hpp` | `value` (lower drawn first, absent means 0) |
| `SpriteAnimation` | `sprite_animation_system.hpp` | `frame_width`, `frame_height`, `frame_count`, `current_frame`, `seconds_per_frame`, `elapsed_seconds` |
| `Kinematic2D` | `kinematic_system.hpp` | `velocity_x/y`, `acceleration_x/y`, `angular_velocity`, `angular_acceleration`, optional `max_speed`, `max_acceleration`, `max_angular_speed`, `max_angular_acceleration` |
| `Collider2D` | `collision_system.hpp` | `geometry` (`Geometry2D`) |

### Coordinate system

`Camera2D::x/y` is the **top-left** corner of the camera in world coordinates,
so world **+y is down**. Gravity is a positive `acceleration_y`. A ground
contact normal pushing the goose upward has a **negative** `normal.y`.

### `AdvanceSpriteAnimations` behaviour

For every entity with both `SpriteAnimation` and `Sprite`, it advances the
frame cursor and then **unconditionally overwrites `sprite.source`** with the
current frame's rect. Consequences:

- Animation always **loops** (`(current_frame + 1) % frame_count`). There is no
  one-shot mode and no "finished" signal. This blocks both the idle fidget and
  the attack lock (§6, §8).
- If you remove the component but leave a stale `sprite.source` behind, the
  next texture gets drawn through the *old* source rect. This is the single
  most likely silent bug in the whole design — see §5.

### `TextureManager` (`include/svanes/render/texture_manager.hpp`)

`LoadTexture(std::string_view)` returns a `TextureHandle`. There is **no
path→handle cache** — the internal map is keyed by an incrementing `uint32_t`
id, so every call re-decodes the file and uploads a brand-new `SDL_Texture`.
Load each file once and share the handle (§9).

### Collision (`include/svanes/collision_system.hpp`)

`DetectCollisions(geometry_a, transform_a, geometry_b, transform_b)` returns
`std::vector<Collision2D>`, where `Collision2D{ normal, penetration_depth }`
and `normal` is the unit direction to move **a** out of **b**.

**There is no collision system.** Nothing iterates the world, nothing builds
candidate pairs, and nothing resolves anything. Detection only, called manually
per pair, by the game. See §7.

---

## 3. The `Goose` class — purpose and shape

The goose is the player. The class exists so `ErikGame::Update` doesn't turn
into a wall of component juggling, and so a second goose (an NPC) is cheap.

Lives in its own `goose.hpp` / `goose.cpp` in this directory — it has enough
logic to need a `.cpp`, which is the bar `/CLAUDE.md` sets for earning a
header.

Rough shape (write the doc comments yourself — this repo forbids generated
comments):

```cpp
enum class GooseState : std::uint8_t {
    Idle,
    Walking,
};

class Goose final {
public:
    void Spawn(svanes::GameContext& context, float x, float y);
    void Update(const svanes::FrameContext& frame, svanes::Vector2D move_intent);
    void SetState(svanes::Registry& world, GooseState next);

private:
    svanes::Entity entity{};
    svanes::TextureHandle idle_texture{};
    svanes::TextureHandle walk_texture{};
    GooseState state = GooseState::Idle;
    float time_in_state = 0.0F;
    bool grounded = false;
    bool spawned = false;
    float speed = 300.0F;
};
```

**`goose.cpp` must be added to `add_executable(svanes_game_erik ...)` in
`/CMakeLists.txt` (line 122)** or it will not be compiled or linked.

---

## 4. Construction: two-phase, deliberately

The engine constructs `ErikGame` before any `GameContext` exists, so a `Goose`
member cannot have a constructor that takes the context. Chosen approach:

```cpp
// erik_game.hpp
Goose goose;

// erik_game.cpp
void ErikGame::Initialize(svanes::GameContext& context)
{
    goose.Spawn(context, 960.0F, 700.0F);
}
```

Rejected alternatives:

- `std::optional<Goose>` + `emplace(context, x, y)` — lets `Goose` have a real
  constructor and never exist half-built, but adds `->` / `.value()` noise at
  every call site.
- `std::unique_ptr<Goose>` — no benefit without polymorphism, and adds an
  allocation.

Chosen because it mirrors the two-phase pattern `IGame` already imposes one
level up. The cost is a window where the goose exists but has no entity;
cover it with the `spawned` flag and an informative crash in `Update`, per the
project's "don't succeed mysteriously" rule.

(Note: `Goose.Initialize()` is not C++ syntax — a static call would be
`Goose::Initialize()` with `::`. But the two-phase issue above is the real
reason a static factory doesn't help here.)

---

## 5. Animation state machine

Idle and walk are **two separate pngs**, not two strips in one sheet. So idle
genuinely has no `SpriteAnimation` component:

```cpp
void Goose::SetState(svanes::Registry& world, GooseState next)
{
    if (state == next) {
        return;
    }
    state = next;
    time_in_state = 0.0F;

    svanes::Sprite& sprite = world.GetComponent<svanes::Sprite>(entity);

    switch (next) {
    case GooseState::Idle:
        sprite.texture = idle_texture;
        sprite.source = std::nullopt;
        world.RemoveComponent<svanes::SpriteAnimation>(entity);
        break;
    case GooseState::Walking:
        sprite.texture = walk_texture;
        world.AddComponent<svanes::SpriteAnimation>(entity, svanes::SpriteAnimation{
            .frame_width = 29,
            .frame_height = 27,
            .frame_count = 4,
            .seconds_per_frame = 0.1F,
        });
        break;
    }
}
```

Three load-bearing details:

1. **The early-out is mandatory.** `Update` calls this every frame. Without the
   guard, `AddComponent` replaces `SpriteAnimation` sixty times a second,
   `current_frame` resets to 0 each time, and the walk cycle freezes on frame 1
   forever.
2. **`sprite.source = std::nullopt` is mandatory.** Removing `SpriteAnimation`
   stops the animation system writing to `source`, but does not clear what it
   last wrote. The idle png (29×27) would be drawn through a walk-sheet source
   rect (e.g. `x=87, w=29`), rendering garbage or nothing, with no error.
3. **No `default:` case.** When a new state is added, the compiler warns about
   the unhandled case instead of silently falling through.

### Where "what is currently playing" lives

Chosen: **an enum member on `Goose`.** Rejected:

- Comparing `sprite.texture.id` to the target texture's id — keeps the ECS as
  the single source of truth and needs no extra state, but breaks silently the
  day idle and walk are packed into one sheet.
- A `const GooseAnimation*` compared by address — zero maintenance when adding
  states, but copying a `Goose` leaves the copy pointing at the original's
  member.

Both failure modes are *silent*, which is why the enum wins despite duplicating
a byte of state.

---

## 6. Command vs. state — where the ErikGame/Goose line falls

**The game commands; the goose decides how that looks.**

`ErikGame` maps keyboard to intent. `Goose` derives its own state from that
intent. Input mapping does *not* belong in `Goose`, because a second goose
driven by AI or the network must work identically.

```cpp
svanes::Vector2D player_move{};
if (frame.input.IsDown(svanes::Key::D)) {
    player_move.x += 1.0F;
}
if (frame.input.IsDown(svanes::Key::A)) {
    player_move.x -= 1.0F;
}

player.Update(frame, player_move);
npc.Update(frame, npc_brain.DesiredMove());
```

An earlier idea — the game calling `player.SetState(GooseState::Walking)`
directly on a keypress — was rejected because:

- Nothing returns the goose to `Idle` on key release, so the game ends up
  owning the rule "not moving means idle", which is goose knowledge.
- `SetState(Walking)` says what the goose *looks like*, not that it moved.
  Either the game also does the moving (and the class buys nothing), or
  `Walking` has to carry a direction it cannot express.
- The idle fidget needs an "idle for N seconds" timer. If the game commands
  state, the game owns that timer, and the goose's most goose-specific
  behaviour leaks into `ErikGame::Update`.

**`SetState` stays public anyway**, just not as the per-frame interface.
Cutscenes, stuns and deaths are genuine commands from the game.

The test to apply: anything `ErikGame` says to the goose should still make
sense if the goose were swapped for a different character.

---

## 7. Physics

### Movement goes through `Kinematic2D`, never `Transform`

`AdvanceKinematics` runs immediately after `game.Update` and integrates
velocity and acceleration into the `Transform`. Writing `transform.x` directly
from game code fights the integrator. `Goose::Update` sets `velocity_x` /
`velocity_y` (or accelerations) and lets the engine move the entity.

This makes the rest nearly free:

- **Gravity** — a persistent positive `acceleration_y`. The docs state
  acceleration persists until changed.
- **Jump** — on the jump command, if `grounded`, set
  `velocity_y = -jump_speed`. One assignment.
- **Fly** — cancel or reduce `acceleration_y` while flying and let the intent's
  y-component drive `velocity_y`. Flying is just movement with a different
  mode; the interesting part is the state transition, not the physics.
- **Terminal velocity** — `max_speed` already exists on the component.

### Open question: where does the gravity constant live?

If `Goose` sets its own gravity, an underwater or low-gravity level has to
reach into the goose to change it. If `ErikGame` sets it at spawn time, gravity
is a property of the world, which is what it actually is. **Leaning toward the
world/`ErikGame`**, but not finally decided.

### Grounded

`grounded` is the most important bit of state in the design — it gates jumping,
distinguishes Idle/Falling, and stops gravity accumulating forever. It has to
be computed during collision resolution, which is currently the game's job.

### The one-frame ordering problem

`game.Update` runs *before* `AdvanceKinematics`, and nothing runs after it. So
collision resolution inside `Goose::Update` is always correcting penetration
caused by *last* frame's integration — the goose visibly sinks into the ground
for a frame before popping out. Not fatal at 60fps. The real fix is an
engine-side `ResolveCollisions(world)` step in the loop after
`AdvanceKinematics` (§10).

---

## 8. Beyond movement: swords, bullets, and state priority

### Not everything needs a class

A bullet is an entity with `Kinematic2D`, `Sprite` and `Collider2D`. Once
spawned it needs no owner — the engine integrates it, and it dies on collision
or when off-screen. There should be **no `Bullet` class**: that would mean
inventing an owner for something that doesn't need one, plus a container and
reaping code.

`Goose` is a class because *you* need a handle to command it, not because
entities require classes. `Goose::Shoot` calls `world.CreateEntity()`, attaches
components, and forgets about it.

A sword swing is a one-shot animation plus a hitbox entity that lives a few
frames — same spawn-and-forget shape, but it hits the problem below.

### Derived vs. locked states

The state machine will grow to roughly `Idle, Walking, Jumping, Falling,
Flying, Attacking`, and these are two different kinds of thing:

- **Derived** — recomputed from scratch every frame. `Walking` if the move
  vector is non-zero, `Falling` if airborne and `velocity_y > 0`. Cheap and
  self-correcting.
- **Locked** — runs to completion and refuses recomputation. A sword swing must
  not turn back into `Walking` halfway through because a movement key was
  tapped.

A purely derived machine cancels the attack animation on the very next frame.
So `SetState` needs a "this state holds until it finishes" concept, which means
knowing when a one-shot animation has finished — the **same engine gap** the
idle fidget needs (§2, §10). Two features wanting the same change is usually
the sign it's worth making.

Shape to use: derive the movement state every frame, but check a lock first and
bail out if one is active. Start with a plain timer for the lock; replace it
with real animation-completion once the engine supports it.

---

## 9. Multiple geese

`Goose` holds an `Entity`, texture handles, an enum and floats — all values, no
references. So it is copyable and movable, and `std::vector<Goose>` works.
(This is why storing a `Registry&` member would have been a mistake: a class
with a reference member isn't assignable and fights every container.)

Two things break with a second goose:

1. **Duplicate textures.** `LoadTexture` has no path cache, so each goose
   loading its own textures uploads another copy of the same png. Fix by
   separating asset loading from spawning:

   ```cpp
   struct GooseAssets {
       svanes::TextureHandle idle;
       svanes::TextureHandle walk;
   };

   GooseAssets LoadGooseAssets(svanes::TextureManager& assets);
   ```

   `ErikGame::Initialize` calls it once and passes the result into each
   `Spawn`. `Goose` then holds copies of the handles and loads nothing itself,
   which also makes `Spawn` cheap enough to call mid-game.

2. **Identical movement**, if each goose reads `frame.input` itself. Already
   solved by the intent design in §6.

---

## 10. Engine gaps worth proposing to the team

Each of these is blocked work for this game and is generally useful, i.e. a
defensible contribution to the shared engine rather than a personal hack.

1. **One-shot animations.** `SpriteAnimation` always loops and never reports
   completion. Needs a `looping` flag and a `finished` flag. Wanted
   independently by the idle fidget and the attack-state lock. Universal:
   attack swings, jumps, hit reactions.
2. **A collision system.** `Collider2D` and `DetectCollisions` exist, but
   nothing iterates the world. Needs (a) a broadphase yielding candidate pairs
   and (b) a `ResolveCollisions(world)` step in the loop *after*
   `AdvanceKinematics`, which also fixes the one-frame lag in §7.
3. **A texture path cache** in `TextureManager`. Currently every `LoadTexture`
   call re-decodes and re-uploads. This is squarely in syllabus feature area 5
   (resource management).
4. **A per-frame force accumulator on `Kinematic2D`.** Acceleration persisting
   until changed forces every game to manually zero it each frame (see
   `orbitalEscalation` lines 243-247, which exist purely for that). The
   standard fix is an accumulator cleared after integration, with persistent
   acceleration (gravity) as a separate field.
5. **Horizontal flip on `Sprite`.** There is no flip field, so a goose walking
   left still faces right.

---

## 11. What `orbitalEscalation` already proves

A teammate's game in `games/orbitalEscalation/` is a collision-physics
showcase and is worth reading before writing any collision code.

**Reuse its detection wholesale.** `DetectCollisions` handles rectangles,
triangles, circles and composite shapes via SAT, and that game is a live stress
test. Do not rewrite it.

**Do not copy its response.** `ApplyCollisionAcceleration`
(`orbital_escalation_game.cpp:30-45`) is a **penalty method**: overlap becomes a
repulsive acceleration (`collision.normal * 10000.0F`), and `penetration_depth`
is ignored entirely. That is right for an orbital sandbox — continuous, bouncy,
composes with attractor fields, no special cases. It is close to the worst
option for a platformer: a goose standing under gravity sinks until the penalty
force balances its weight and then oscillates around that depth, and raising
the constant to stop the jitter makes the integrator explode. It also never
yields a clean `grounded` boolean.

**What this game wants instead:** positional correction. Move the goose out
along `collision.normal` by `penetration_depth` (the field orbital ignores) and
zero the velocity component into the surface. Pass the goose as `a`, and
grounded falls out of the normal direction — standing on ground pushes upward,
so `normal.y` is negative in this y-down world. Crisp per-frame boolean, no
springiness, and it's what gates the jump.

**The architectural point:** two games, one detection routine, two
*incompatible* responses. That is the evidence for where the engine boundary
belongs — detection and broadphase in the engine, response policy chosen by the
game (or selected from a couple of engine-provided policies). Worth agreeing on
jointly rather than either party asserting it, and worth writing up for a class
about engine architecture.

Also visible there: the collision pair list is hand-enumerated
(`orbital_escalation_game.cpp:249-257`), so adding an entity means editing the
list. That is the missing broadphase, and a platformer with dozens of platforms
needs it more than an orbital sandbox with five bodies does.

**Open question for the team:** is the author of that game planning to write a
collision *system*? If yes, build the goose against a hardcoded ground line and
swap later. If no, that is an available contribution with a clear owner.

---

## 12. Suggested build order

1. Delete the broken `class Goose` sketch from `erik_game.cpp`, get the current
   file compiling again (`just build`) — this has not been verified since the
   `SolidShape` fix.
2. `goose.hpp` / `goose.cpp` with `Spawn` and `Update`; add `goose.cpp` to
   `CMakeLists.txt:122`. Movement via `Kinematic2D` velocity, no state machine
   yet.
3. The `GooseState` enum and `SetState`, with idle/walk swapping.
4. Gravity plus a hardcoded ground line — this gets jumping working before any
   collision code exists.
5. Real collision, replacing the ground line, once the shape of the collision
   system is settled with the team.
6. `GooseAssets` when a second goose actually appears. Not before.

## Build commands

`just build`, `just run`, `just release`, `just clean`, `just check-deps`,
`just fetch-deps`. All work identically on Windows, macOS and Linux. New
translation units must be added to `add_executable(svanes_game_erik ...)` in
the root `CMakeLists.txt`.
