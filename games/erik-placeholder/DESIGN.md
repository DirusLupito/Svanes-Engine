# Erik's game — design notes and plan

Working notes for the goose game built on the Svanes engine. Written so this
design conversation can be resumed on another machine with no other context.

Read `/CLAUDE.md` at the repo root first — its coding rules override anything
here. Most relevant: **no comments in code** (Erik writes those himself), no
`new`/`delete`, fixed-width integer types, braces on every block, crash loudly
rather than succeed mysteriously, and earn your complexity before reaching for
a language feature.

---

## 0. Status

**Deadline: iteration 1 is due tonight.** Section 8 has the requirements and
section 9 has the critical path. Everything in sections 3–7 is design context
for that work and for what comes after.

Done so far:

- Game loop order swapped so `AdvanceKinematics` runs *before* `game.Update`
  (`src/application.cpp:102-103`). Rationale and cost in §2.

Not done yet — nothing else has been written or compiled.

Known broken: `erik_game.cpp:11-30` holds a half-written `class Goose` that is
not valid C++ (statements at class scope, references to a `context` that isn't
in scope). It is a sketch, not code. **Delete it first** — nothing compiles
until it's gone.

---

## 1. The game

Side-view 2D, Terraria-boss-fight flavored, bullet-hell combat. The player is a
goose. Weapons are a sword (one-shot swing animations), a gun, and eventually a
couple of magical weapons. Enemies and bosses fire bullets too.

Art: only the goose uses sprites for now. Everything else is solid-color shapes
and gets art later.

---

## 2. Engine facts this design depends on

All verified by reading the engine source on 2026-09-07. Re-check before
relying on them — the engine is under active development by the team.

### The frame loop (`src/application.cpp`)

```
game.Initialize(game_context)          // once, line 58

// per frame:
AdvanceKinematics(world, delta)        // line 102
game.Update(frame_context)             // line 103
SynchronizeTextInput(...)              // line 105
AdvanceSpriteAnimations(world, delta)  // line 106
game.ShouldQuit()                      // line 108
SubmitShapes(...)                      // line 125
SubmitSprites(...)                     // line 126
```

**The order of the first two lines was changed by us.** It used to be
`game.Update` then `AdvanceKinematics`.

Why: collision resolution lives in game code. Integrating first means `Update`
observes *this* frame's penetration and can correct it before anything is
drawn. Under the old order the game corrected penetration and the integrator
immediately re-penetrated, and *that* was the state that got rendered.

Tracing a landing at 800 px/s at 60fps under the new order:

- `AdvanceKinematics` — goose moves 12.8px, ends 7.8px inside the ground
- `Update` — detects depth 7.8, pushes out along the normal, zeroes
  `velocity_y`, sets `grounded`
- render — goose sits exactly on the ground surface

Resting is stable too: gravity only accumulates one frame of velocity between
corrections, so residual penetration is sub-pixel instead of the ~13px
sink-and-pop of the old order.

Cost: **one frame (~16ms) of input latency.** Velocity set from input in
`Update` isn't integrated until the next frame. Jumping feels one frame late.
Accepted as the better trade.

Bonus: `Update` now sees post-integration transforms, so a camera that follows
the goose matches the rendered position exactly — no one-frame camera lag.

Animations still run *after* `game.Update`, which is correct: `Update` may swap
animation state, and `AdvanceSpriteAnimations` needs to write `sprite.source`
before the render.

### Contexts (`include/svanes/game.hpp`)

- `GameContext{ world, assets, camera, output_width, output_height, scale_mode& }`
  — passed to `Initialize` only. The only place `TextureManager` is reachable.
- `FrameContext{ world, input, delta_seconds, output_width, output_height, camera, scale_mode& }`
  — passed to `Update` as a `const&`. The reference members are still writable
  through the const struct, which is why `frame.scale_mode = ...` compiles.

`IGame` is two-phase: the game object is constructed with no context and only
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

Gotchas, each of which will bite silently:

1. **Keyed by type, so an entity has at most ONE component of a given type.**
   No entity can carry two `Collider2D`s. This is the structural reason
   hitboxes have to be separate entities (§6).
2. **Do not call `DestroyEntity` inside a `ForEach`.** It mutates the map being
   iterated. Collect victims into a `std::vector<Entity>` and destroy them
   after the loop. This matters immediately for bullets.
3. **Do not cache component references across frames.** `AddComponent`
   reassigns the `std::any` slot, which can relocate the contained object and
   leave a stored reference dangling. Look components up fresh each frame.
4. `GetComponent` on a missing component goes through `.at()` and
   `std::any_cast`, so it throws `std::out_of_range` or `std::bad_any_cast`
   with no useful context. Guard with an explicit check and a real message.

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
| `PointAttractor2D` | `attractor_system.hpp` | `accelerationField`, optional `cutoff_radius` |

### Geometry

`Geometry2D` is a variant over `Rectangle2D`, `Triangle2D`, `Circle2D`,
`ConvexPolygon2D`, and `CompositeShape2D`.

**`Rectangle2D` is center-based** — `x`/`y` are the *center*, and they act as a
local offset added to the entity's `Transform`. So a `Transform{960, 1000}`
with `Rectangle2D{0, 0, 1920, 80}` spans y from 960 to 1040. Getting this
backwards puts the ground half a ground-height off.

### Coordinate system

`Camera2D::x/y` is the **top-left** corner of the camera in world coordinates,
so world **+y is down**. Gravity is a positive y. A ground contact normal
pushing the goose upward has a **negative** `normal.y`.

### Kinematics (`src/kinematic_system.cpp`)

`AdvanceKinematics` integrates acceleration into velocity into `Transform`.
**Acceleration persists until changed** — set it once and it keeps applying.

`orbitalEscalation` zeroes accelerations every frame
(`orbital_escalation_game.cpp:293-297`) only because it accumulates penalty
forces into them. We do not do that, so we do not need that loop.

`max_speed` clamps the **2-norm of the whole velocity vector**, so using it for
terminal fall speed would also throttle horizontal running mid-fall. For a
platformer, clamp `velocity_y` directly instead.

`EvaluateAttractors` runs inside `AdvanceKinematics` every frame regardless. We
use no attractors, so it returns an empty map and costs nothing.

There is a `// TODO: Implement global acceleration fields, like a downward
gravitational field.` at `src/kinematic_system.cpp:98`. That is exactly the
gravity work in §7.

### Collision (`include/svanes/collision_system.hpp`)

`DetectCollisions(geometry_a, transform_a, geometry_b, transform_b)` returns
`std::vector<Collision2D>`, where `Collision2D{ normal, penetration_depth }`
and `normal` is the unit direction to move **a** out of **b**.
`penetration_depth` is the distance along that normal to reach touching.

Handles rectangles, triangles, circles, arbitrary convex polygons, and
composites, via SAT choosing the minimum-translation axis. It is well tested by
`orbitalEscalation`. **Reuse it; do not rewrite it.**

Two limits worth knowing:

- **There is no collision system.** Nothing iterates the world, nothing builds
  candidate pairs, nothing resolves anything. Detection only, called manually
  per pair, by the game.
- **Composite results are a flat vector with no part identification.** You
  cannot tell *which* part of a composite was hit. Combined with the one
  `Collider2D` per entity limit, this means hitbox/hurtbox separation needs
  separate entities (§6).

### Sprite animation

`AdvanceSpriteAnimations` advances the frame cursor for every entity with both
`SpriteAnimation` and `Sprite`, then **unconditionally overwrites
`sprite.source`** with the current frame's rect. Consequences:

- Animation always **loops** (`(current_frame + 1) % frame_count`). There is no
  one-shot mode and no "finished" signal. This blocks the idle fidget, the
  attack lock, and knockback (§10).
- If you remove the component but leave a stale `sprite.source`, the next
  texture gets drawn through the *old* source rect. Silent garbage. See §5.

`Sprite` has **no horizontal flip field**, so a goose walking left still faces
right. Facing can be tracked today but not displayed (§10).

### Rendering and screen/world conversion

The render chain is `screen = (world - camera) * zoom * scale + offset`, where
`scale` and `offset` come from `ComputeScale`/`ComputeOffset` in
`src/render/render_system.cpp` against a 1920x1080 design resolution.

`Camera2D::ScreenToWorld` **only inverts the camera and zoom** — it never
undoes `scale`/`offset`. Under `ScaleMode::Constant` those are 1 and 0 so it is
correct; under `ScaleMode::Proportional` it is wrong. Since Tab-toggling scale
modes is a hard requirement and the goose aims at the mouse, **aiming would
break exactly when the scaling feature is demoed.** `ComputeScale` and
`ComputeOffset` are `static`, so a game cannot correct for this today. Fix in
§7.

### Textures (`include/svanes/render/texture_manager.hpp`)

`LoadTexture(std::string_view)` returns a `TextureHandle`. There is **no
path→handle cache** — the map is keyed by an incrementing `uint32_t`, so every
call re-decodes the file and uploads a brand-new `SDL_Texture`. Load each file
once and share the handle (§5).

### Input (`include/svanes/input.hpp`)

Available keys include `Escape`, `Space`, `Tab`, `W`/`A`/`S`/`D`, `Q`/`E`/`R`/`F`,
arrows, `LeftShift`, and mouse `Left`/`Right`. `IsDown`, `WasPressed`,
`MousePosition()`, `MouseWheelThisFrame()`.

### Build

- `just build erik` builds **only** the erik target, and `just run erik` builds
  and launches it. Chris's game is stale and does not compile, but it **cannot
  block us** because of this.
- New translation units must be added to
  `add_executable(svanes_game_erik ...)` at `/CMakeLists.txt:124`.
- `ERIK_GAME_ASSETS_DIR` is defined for the erik target and points at
  `games/erik-placeholder/assets`.

---

## 3. Architecture: body vs brain

The core decision, and the thing that resolves the "what do I even call it"
problem.

There are **two** abstractions, not one:

- **The body** — health, speed, facing, inventory, animation. The player goose,
  an enemy, a boss, and an NPC all have one. Agreed name: **`Character`**.
- **The brain** — produces intent. Keyboard today, AI later, network packets
  after that.

Everything on the original "what should the goose class handle" list (health,
walk/run speed, facing, inventory, animation state) is *body*. None of it is
decision-making. Conflating the two is why no single name fit.

**The split is already built.** `Goose::Update(frame, move_intent)` means the
goose has no idea where intent comes from. That is the whole Pawn/Controller
pattern for the price of one parameter. A formal `IController` hierarchy buys
nothing until a real AI exists.

The game commands; the goose decides how that looks:

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

Input mapping does *not* belong in `Goose` — a second goose driven by AI or the
network must work identically.

The test to apply: anything `ErikGame` says to the goose should still make
sense if the goose were swapped for a different character.

`SetState` stays public anyway, just not as the per-frame interface. Cutscenes,
stuns and deaths are genuine commands from the game.

---

## 4. Where data lives: components vs class members

**Rule: does anything outside the goose need to read or write this?**

The decisive argument is bullet-hell damage. If `health` is a member of
`Goose`, a bullet holding an `Entity` id must find the `Goose` C++ object that
owns it — which means maintaining an `unordered_map<Entity, Goose*>` that
breaks the moment a `std::vector<Goose>` reallocates. Repeat per enemy class.

If `health` is a component, the entire damage system is:

```cpp
world.GetComponent<Health>(hit_entity).current -= damage;
```

One line, works on anything with the component, no map, no ownership question.
With dozens of bullets hitting dozens of things per second, this settles it.
The same reasoning covers networking later: you sync by serializing components,
not by walking a C++ object graph.

**Components** (registry-owned, other systems touch them): `Health`, `Facing`,
`Inventory`, and a `Team`/faction if one becomes necessary.

**Class members** (nobody else needs them): the `Entity` handle, cached
`TextureHandle`s, the animation state enum, `time_in_state`, `grounded`, and
tuning constants like walk/run speed.

This is not extra complexity — a `Health` struct of two floats is not more
complex than two floats in a class. It is the same data on a better shelf. Per
`/CLAUDE.md`, these bare structs live in the header of the system that uses
them, not in their own files.

---

## 5. The `Goose` class

Lives in its own `goose.hpp` / `goose.cpp` — it has enough logic to need a
`.cpp`, which is the bar `/CLAUDE.md` sets for earning a header.

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

**Add `goose.cpp` to `add_executable(svanes_game_erik ...)` at
`/CMakeLists.txt:124`** or it will not be compiled or linked.

### Two-phase construction, deliberately

The engine constructs `ErikGame` before any `GameContext` exists, so a `Goose`
member cannot have a constructor taking the context:

```cpp
// erik_game.hpp
Goose goose;

// erik_game.cpp
void ErikGame::Initialize(svanes::GameContext& context)
{
    goose.Spawn(context, 960.0F, 700.0F);
}
```

Rejected: `std::optional<Goose>` + `emplace` (adds `->`/`.value()` noise at
every call site), and `std::unique_ptr<Goose>` (no benefit without
polymorphism, plus an allocation).

Chosen because it mirrors the two-phase pattern `IGame` already imposes one
level up. The cost is a window where the goose exists but has no entity; cover
it with the `spawned` flag and an informative crash in `Update`.

### Animation state machine

Idle and walk are **two separate pngs**, not two strips in one sheet, so idle
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
   stops the animation system writing to `source` but does not clear what it
   last wrote. The idle png (29x27) would be drawn through a walk-sheet source
   rect, rendering garbage, with no error.
3. **No `default:` case.** A new state then produces a compiler warning instead
   of silently falling through.

"What is currently playing" lives in **an enum member on `Goose`**. Rejected:
comparing `sprite.texture.id` against the target texture (breaks silently the
day idle and walk get packed into one sheet), and a `const GooseAnimation*`
compared by address (copying a `Goose` leaves the copy pointing at the
original's member). Both failure modes are *silent*, which is why the enum wins
despite duplicating a byte of state.

### Movement goes through `Kinematic2D`, never `Transform`

`Goose::Update` sets `velocity_x`/`velocity_y` (or accelerations) and lets the
engine move the entity. Writing `transform.x` directly fights the integrator.

- **Gravity** — comes from the world (§7), not the goose.
- **Jump** — on the jump command, if `grounded`, set `velocity_y = -jump_speed`.
- **Fly** — set `gravity_scale` to 0 while flying and let the intent's
  y-component drive `velocity_y`.
- **Terminal velocity** — clamp `velocity_y` directly, not via `max_speed`.

### Collision response: positional correction

**Do not copy `orbitalEscalation`'s `ApplyCollisionAcceleration`.** It is a
penalty method — overlap becomes a repulsive acceleration
(`collision.normal * 10000.0F`) and `penetration_depth` is ignored entirely.
That is right for an orbital sandbox: continuous, bouncy, composes with
attractor fields, no special cases. It is close to the worst option for a
platformer — a goose standing under gravity sinks until the penalty force
balances its weight, then oscillates, and raising the constant to stop the
jitter makes the integrator explode. It also never yields a clean `grounded`.

**This game uses positional correction:** move the goose out along
`collision.normal` by `penetration_depth`, and zero the velocity component into
the surface. Pass the goose as `a`. `grounded` then falls out of the normal
direction — ground pushes upward, so `normal.y < 0` in this y-down world.

Iterate the goose against every other `Collider2D` with `ForEach`, skipping
itself — about ten lines, no hand-maintained pair list like orbital's
(`orbital_escalation_game.cpp:299-313`), and it demonstrates the collision
system more convincingly.

**The architectural point, worth the writeup:** two games, one detection
routine, two *incompatible* responses. That is the evidence for where the
engine boundary belongs — detection and broadphase in the engine, response
policy chosen by the game.

---

## 6. Bullets, weapons, and combat

### No `Bullet` class

A bullet is an entity with `Transform`, `Kinematic2D`, `SolidShape`,
`Collider2D`, and a `Bullet` component. Once spawned it needs no owner — the
engine integrates it, and it dies on collision or expiry. A class would mean
inventing an owner for something that doesn't need one, plus a container and
reaping code.

`Goose` is a class because *you* need a handle to command it, not because
entities require classes.

```cpp
struct Bullet {
    svanes::Entity owner;
    float remaining_seconds;
};
```

- `gravity_scale = 0` so bullets fly straight.
- The `owner` field solves friendly fire for **both** sides with no `Team`
  concept — a bullet skips its owner and skips other bullets.
- The goose aims at the mouse world point; the enemy fires a fixed direction.
- Both need a **fire cooldown** or they spawn 60 bullets a second.
- **Collect expired/hit bullets into a vector and `DestroyEntity` after the
  `ForEach`,** never inside it.

### Hitboxes vs hurtboxes

The shape you are *hit* on is usually smaller than your sprite, and a sword
swing is a shape that exists for a few frames. But an entity can hold exactly
one `Collider2D`, and composite results carry no part identification (§2). So
**hitboxes must be separate entities** whose transforms the owner updates each
frame. There is no parent/transform hierarchy component — `ComposeTransforms`
exists as a free function, but parenting is manual.

### Derived vs locked states

The state machine grows to roughly `Idle, Walking, Jumping, Falling, Flying,
Attacking`, and these are two different kinds of thing:

- **Derived** — recomputed from scratch every frame. `Walking` if the move
  vector is non-zero, `Falling` if airborne and `velocity_y > 0`. Cheap and
  self-correcting.
- **Locked** — runs to completion and refuses recomputation. A sword swing must
  not turn back into `Walking` because a movement key was tapped. Knockback is
  the same shape: you don't get to steer during it.

A purely derived machine cancels the attack on the very next frame. So
`SetState` needs a "this state holds until it finishes" concept, which needs to
know when a one-shot animation has finished — the **same engine gap** the idle
fidget needs. Three features wanting the same change is a strong signal it is
worth making.

Shape to use: derive the movement state every frame, but check a lock first and
bail out if one is active. Start with a plain timer for the lock; replace it
with real animation-completion once the engine supports it.

### Inventory

A real inventory, but small — sword, gun, and a couple of magical weapons.
Never more than fits on the number keys, so number keys select slots directly
and a fixed-size array is the right container. It is a component (§4).

The weapon, not the goose, should probably own its attack animation and hitbox
spawning. Deferred until there is more than one weapon.

### Multiple geese

`Goose` holds an `Entity`, texture handles, an enum and floats — all values, no
references. So it is copyable and movable, and `std::vector<Goose>` works.
(This is why a `Registry&` member would have been a mistake: a class with a
reference member isn't assignable and fights every container.)

Two things break with a second goose:

1. **Duplicate textures**, since `LoadTexture` has no path cache. Fix by
   separating asset loading from spawning:

   ```cpp
   struct GooseAssets {
       svanes::TextureHandle idle;
       svanes::TextureHandle walk;
   };

   GooseAssets LoadGooseAssets(svanes::TextureManager& assets);
   ```

   `ErikGame::Initialize` calls it once and passes the result into each
   `Spawn`. Not needed until a second goose actually exists.
2. **Identical movement**, if each goose reads `frame.input` itself. Already
   solved by the intent design in §3.

---

## 7. Engine changes we are making

### Global directional gravity

The requirement is that *the engine* provides a directional gravitational
force. Setting `acceleration_y` per entity from game code is the *game*
implementing gravity, so this is a genuine gap, and
`src/kinematic_system.cpp:98` already marks the spot.

Design, mirroring how `scale_mode` already works:

- A `Vector2D gravity` owned by the game loop, exposed through both
  `GameContext` and `FrameContext` as a reference. Defaults to `{0, 0}`, so
  `orbitalEscalation` and `chris` are unaffected. Each game states its choice
  with one line in `Initialize`.
- `AdvanceKinematics(world, delta_seconds, gravity)` applies it at the TODO.
- `Kinematic2D` gains one field: `float gravity_scale = 1.0F`.

`gravity_scale` is the opt-out, and it answers the writeup question about which
entities gravity affects: the ground has no `Kinematic2D` at all, the
path-following enemy is excluded, the goose is `1.0`, bullets are `0.0`. One
field instead of a tag component, and it gives floaty (`0.5`) for free later.

### Correct screen-to-world for a point

Needed because the goose aims at the mouse and the scale-mode toggle is a
requirement (§2). One function in `render_system.hpp`:

```cpp
Vector2D ScreenToWorldPoint(const Camera2D& camera, Vector2D screen_point,
                            std::int32_t output_width, std::int32_t output_height, ScaleMode mode);
```

It undoes `offset`, then `scale`, then defers to the camera. The game never
sees the transform chain.

---

## 8. Iteration 1 requirements (due tonight)

### Engine

- [x] Open an SDL3 window and render to the display
- [x] A generic entity system representing the objects in your game
- [ ] **A physics system which provides a directional gravitational force** —
      the one real gap, see §7
- [x] An input handling system for controlling the window and game entities
- [x] A collision detection system detecting when entities collide/overlap
- [x] A system to scale the window size and switch between scaling modes

### Game

- [x] Call the engine's initialization and game loop functions
- [ ] Three specific objects: a **static** object (ground), a **controllable**
      object (the goose), and an **auto-moving** object following a
      continuous predefined path (the enemy)
- [ ] Use the engine's physics system (gravity)
- [ ] Decide which entities are affected by gravity, explain in writeup
- [ ] Use the input system for player controls, discuss in writeup
- [ ] Use collision detection between game objects and **resolve** the
      collisions, explain rationale in writeup
- [x] Demonstrate the scaling feature, toggling modes on a designated key (Tab)

The writeup is spoken, not written, and does not need preparation.

### What tonight does NOT require

The requirements are far smaller than the design above. Not needed tonight:
the sword, weapons, inventory, facing/flip, one-shot animations, the idle
fidget, `Health`, and the `Character` base class.

Note that the third object is *"an auto-moving object that follows a
continuous, predefined path"* — that is **not a character**. No health, no
animation, no state machine, no brain. So the requirement itself confirms the
deferral in §11: there is still no second real character to extract a base
class from.

Bullets are not strictly required either, but are being added because the game
wants them and they validate the "no `Bullet` class" decision.

---

## 9. Critical path for tonight

1. **Delete the broken `Goose` sketch** at `erik_game.cpp:11-30`. Nothing
   compiles until this is gone.
2. **Engine gravity** (§7) — self-contained, unblocks the goose being physical.
   One line added to each game's `Initialize`.
3. **Ground** — static entity, `SolidShape` + `Collider2D`, no `Kinematic2D`.
4. **Goose** — `goose.hpp`/`goose.cpp`: spawn, A/D movement, Space to jump,
   gravity, ground resolution via positional correction, idle/walk animation
   swap. Add `goose.cpp` to `CMakeLists.txt:124`. **This is the long pole.**
5. **Enemy** — solid-color rectangle, position from a sine or waypoint lerp,
   `Collider2D`.
6. **`ScreenToWorldPoint`** (§7) — needed before aiming works correctly.
7. **Bullets** — the `Bullet` component, goose fires at the mouse world point,
   enemy fires a straight line, cooldowns on both, deferred destruction.
8. **Goose/enemy collision** — needs to visibly *do* something; knockback is
   the cheapest honest answer. `Health` only if time allows.

Roughly a 2–3 hour path with the goose as the long pole. Verify with
`just run erik`.

---

## 10. Engine gaps worth proposing to the team

Each is blocked work for this game and generally useful — a defensible
contribution to the shared engine rather than a personal hack.

1. **One-shot animations.** `SpriteAnimation` always loops and never reports
   completion. Needs a `looping` flag and a `finished` flag. Wanted
   independently by the idle fidget, the attack lock, and knockback.
2. **A collision system.** `Collider2D` and `DetectCollisions` exist, but
   nothing iterates the world. Needs a broadphase yielding candidate pairs and
   a `ResolveCollisions(world)` step. Note the loop-order change in §2 already
   fixed the ordering half of this problem.
3. **Horizontal flip on `Sprite`.** No flip field, so a goose walking left
   still faces right. Blocks displaying `Facing` at all.
4. **A texture path cache** in `TextureManager`. Every `LoadTexture` re-decodes
   and re-uploads. Squarely in syllabus feature area 5 (resource management).
5. **A per-frame force accumulator on `Kinematic2D`.** Acceleration persisting
   until changed forces games to manually zero it each frame (see
   `orbital_escalation_game.cpp:293-297`, which exists purely for that). The
   standard fix is an accumulator cleared after integration, with persistent
   acceleration as a separate field. The gravity work in §7 is a partial step.
6. **Part identification in composite collision results**, so hitbox/hurtbox
   can live on one entity instead of several.

**Open question for the team:** is the author of `orbitalEscalation` planning
to write a collision *system*? If yes, build the goose against the simple
`ForEach` resolution and swap later. If no, that is an available contribution
with a clear owner.

---

## 11. Deferred, and why

| Thing | Why it waits |
|---|---|
| `Character` base class | Only one real character exists. A base designed from one example is a guess; tonight's enemy is a path-follower with no health, animation, or brain, so it is not a second data point. Write the first real enemy concretely, *then* extract what is genuinely shared. |
| `IController` hierarchy | The intent parameter (§3) already provides the decoupling. A hierarchy needs an actual AI to be designed against. |
| Inventory beyond a fixed array | Needs an actual item to exist first. |
| Weapons owning their own hitboxes | Needs more than one weapon. |
| `Facing` display | Blocked on engine sprite flip (§10). |
| One-shot animation states | Blocked on the engine gap (§10). Use plain timers until then. |
| `GooseAssets` | Needed only when a second goose appears. |
| Anything network-shaped | Later stage of the class. The component-not-member decision in §4 is the preparation that matters. |

The general principle: build the concrete thing, let the second instance reveal
the abstraction, and don't design the base class from one example.
