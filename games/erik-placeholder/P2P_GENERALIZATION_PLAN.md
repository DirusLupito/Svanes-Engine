# P2P generalization: what moves into the engine

Planning only; nothing here is implemented. Read alongside `P2P_JOINING_PLAN.md`.

## Goal

Move the reusable parts of the goose game's peer-to-peer code into the engine, so
other games (mainly orbitalEscalation) can use them. Goose-specific parts stay in
the game. The goose game must keep working after every step of the move.

We are not writing networking for the other games. doubleTime (client/server) is
out of scope.

## Features, and where each one goes

### Networking basics

| Feature | What it does | Now | Goes to |
|---|---|---|---|
| Reliable delivery | Resends until the peer confirms, detects dead peers | `NetworkSession` | Engine (already there) |
| Text and address encoding | Writes strings and IP:port into messages | `WriteGooseText`, `WriteGooseAddress` | Engine |
| State hash | Turns bytes into one number to compare worlds | Duplicated in `goose_rollback.cpp` and `goose_simulation.cpp` | Engine |
| Local port | Knowing which port the pipe listens on | `GooseBoundPipe` | Engine (`UdpMsgPipe::LocalPort()`) |

### Membership: who is in the world

| Feature | What it does | Now | Goes to |
|---|---|---|---|
| Joining through any member | Contact a peer, wait, receive an id and everyone's addresses | `GooseJoin`, `HandleJoinRequests` | Engine |
| Join admission | Reject for incompatible build, full world, or a leaving contact | `HandleJoinRequests` | Engine, with game-supplied extra rules |
| Shared, never-reused ids | Every peer hands out the same next id | `next_peer_id` | Engine |
| Roster change agreement | Freeze, agree on a boundary, confirm matching worlds, then add/remove players together | `BeginRosterPause`, `UpdateRosterPause` | Engine |
| Leaving | Graceful departure through the same agreement | `RequestLeave`, `CanClose` | Engine |
| Snapshot transfer | Split a large blob into chunks and rebuild it | `SendSnapshot`, `TakeSnapshot` | Engine |
| Roster revisions | Keep messages from before and after a roster change apart | `GooseNetwork::Update` / `Broadcast` | Engine |
| Relayed addresses | Replace a relayed `127.x` address with the relaying peer's host | `ResolveRelayedAddress` | Engine |

### Input sync and rollback

| Feature | What it does | Now | Goes to |
|---|---|---|---|
| Per-tick input exchange | Every peer sends its input for each tick | `ReceiveMessages`, `RecordInput` | Engine |
| Prediction | Guess a missing input by repeating the last one | `InputFor` | Engine; the game adjusts the guess (never repeat a dash) |
| Only roll back when it mattered | Ask the game whether a different input could have changed that tick | `ChangesStep`, `GooseIntentUse` | Engine stores what the game reports; the game answers the question |
| History, rollback, replay | Restore an earlier world and resimulate after a wrong guess | `history`, `AdvanceTick`, correction in `Update` | Engine |
| Pacing | Slow down when too far ahead of the others | `PacingStepTics` | Engine |
| Fixed-step timing | Real time to ticks, with a cap on catch-up | `pending_tics`, `StepsPerFrame` | Engine |
| Periodic hash checks | Compare world hashes every 100 ticks to catch desyncs | `CheckStateHashes` | Engine |
| Join snapshot envelope | Package roster, next id, tick, world, and hash for a joiner, then verify on arrival | `EncodeJoinSnapshot`, joining constructor | Engine; the world bytes come from the game |

Setting prediction to zero turns rollback into plain lockstep: the game never runs
ahead, so it never rolls back. That makes this one layer usable by a physics-heavy
game like orbitalEscalation, where replaying many ticks would be too expensive.

### Stays in the goose game

- `GooseIntent`, its encoding and validation.
- Local input sampling: double-tap dash, relative aim.
- The rule that aim only matters when firing (`ChangesStep`'s contents).
- What the world contains (geese, enemy, bullets) and how it is rebuilt.
- Spawn points, the player cap value, the rules hash contents, ports, command line.

### Small engine helpers the move needs

- **Component encoding** for engine components: `Transform`, `Kinematic2D`,
  `Timeline`, `SpriteAnimation`, `Color`, geometry. The goose game already writes
  most of these by hand.
- **Full `Timeline` save/restore.** Today the goose game can only rebuild
  unpaused, unscaled, unparented timelines, because the state is private.
- **Stable ids.** A `StableId` component and "iterate in stable-id order", since
  `Registry::ForEach` order differs between peers. Bullet ids and `owner_key` are
  the goose game's hand-made version of this.

## Interfaces the game fills in

The engine doesn't know what a goose is, so it asks the game.

**Synchronized simulation.** This is the "here's what needs to be replicated"
interface: `SyncedSimulation<Input, Use>`, templated on the game's input type and
its "which parts were read" type. The game's simulation gets the `Registry` once
at construction, not on every call.

- **Save the world as bytes / load the world from bytes.** The engine uses the same
  bytes for rollback history, hash checks, and the joiner's snapshot, so what
  peers compare is exactly what a joiner receives.
- **Advance one tick** with every player's input, reporting which parts of each
  input were read.
- **Sample local input** for the next tick. Called once per new tick, never during
  replay, so stateful sampling (double-tap dash) lives here in the game.
- **Encode / decode an input** for sending.
- **Add or remove a player** at a roster change.
- Optional: **predict a missing input** (default repeats the last one), and
  **could this input difference have changed the tick** (default: any difference).

`Input{}` means nothing pressed. The engine uses it while finishing up to a roster
boundary, and a game can return it from sampling while its menu is open.

Game-side wiring, roughly:

```cpp
auto pipe = std::make_unique<svanes::UdpMsgPipe>(port);
peers = join_address ? std::make_unique<svanes::PeerGroup>(std::move(pipe), *join_address, settings)
                     : std::make_unique<svanes::PeerGroup>(std::move(pipe), settings);
sync = std::make_unique<svanes::InputSync<GooseIntent, GooseIntentUse>>(*peers, *simulation, sync_settings);
// each frame: sync->Update(frame);  (it calls peers->Update(), and PeerGroup
// calls back into it as the attached RosterParticipant)
```

**Admission policy** (optional). Extra reasons to refuse a join, such as "match in
progress". The engine already handles version mismatch, full world, and a
leaving contact. Deferred until a game needs one.

**Rules hash.** The game describes what must match between builds (gravity, step
size). The engine adds its own settings and checks the result when someone joins.

A game that doesn't sync by inputs skips the synchronized simulation interface and
uses only membership and snapshot transfer.

## Engine problems found along the way

- **A stray packet can crash a peer.** Any datagram without the protocol marker
  makes `NetworkSession::Update` throw. Garbage from strangers should be logged
  and dropped. Malformed data from a real peer can still crash.
- **Joiners can't talk through the session.** `GooseJoin` encodes packets by hand
  with a made-up peer id. The session should support a "not yet admitted" mode.
- **Message types can collide.** Once the engine sends its own messages, it needs
  a reserved range of message types.
- **Attractor forces depend on `ForEach` order.** `attractor_system.cpp` sums
  forces in registry order, and float sums change with order, so peers could
  disagree. Not fixed; the goose game doesn't use attractors.
- **Cross-platform desyncs are likely.** Clang on Apple Silicon fuses multiply-add
  operations by default, and `std::sin`/`std::hypot` differ between platforms.
  A Mac peer will probably desync from Linux or Windows. Fix below.

## Decisions

- **The world is saved as bytes every tick.** History, hash checks, and the
  joiner's snapshot all use the same bytes. The goose world is under 1 KB, so the
  cost is small, and the engine never needs to know the game's snapshot type.
- **Disconnects come after the move** and are written in the engine's membership
  code (step 5 below).
- **Names:** `PeerGroup` (membership), `PeerJoin` (joiner handshake), `InputSync`
  (input exchange and rollback), `SyncedSimulation` (the interface the game fills in).
- **Sync options slot in.** `InputSync` is one option built on `PeerGroup`; another
  would be a different class taking a `PeerGroup&`, chosen by which one the game
  constructs. `PeerGroup` asks the sync option through a small interface during
  roster changes: ready at a boundary, world hash, snapshot out/in, add/remove player.
- **Cross-platform determinism gets fixed**, since the effort is small:
  - Add `-ffp-contract=off` for GCC and Clang (MSVC's default `/fp:precise`
    already doesn't fuse). It goes in `CMakeLists.txt` next to the warning flags,
    as `target_compile_options(svanes_engine PUBLIC -ffp-contract=off)`. `PUBLIC`
    carries it to every game that links the engine, so the games' simulation code
    gets it too.
  - Add `include/svanes/deterministic_math.hpp` (with a `.cpp`) holding
    `Length(x, y)`, `Sin`, and `Cos`, built only from `+`, `*`, `/`, and
    `std::sqrt`, which give the same result on every platform. Accuracy near
    `std::sin` (about 6 decimal places) is enough.
  - Replace `std::hypot` with `Length`. There are 9 uses across
    `collision_system.cpp`, `kinematic_system.cpp`, `goose.cpp`, `enemy.cpp`,
    `bullets.cpp`, and `goose_simulation.cpp`.
  - Use `Sin`/`Cos` in the geometry code and the enemy's path. Geese never rotate,
    so for now only the enemy path (`goose_simulation.cpp:191`) actually matters.
  - Verify with a peer on Windows or macOS against a Linux peer.
- **Pausing is only allowed with no other members.** `InputSync` gets a local
  pause that throws if anyone else is in the roster. While paused, the network
  keeps running and elapsed time is thrown away, not saved for later. With peers,
  the game's menu opens but the simulation keeps going, and the game samples
  neutral input while the menu is open. A join that arrives while paused goes
  through as normal. Afterward there is a peer, so the pause ends.

## How we work through it

- One step at a time. After each step, run the headless multiplayer test, then
  stop for review before starting the next.

## Order of work

Each step leaves the goose game working, verified with the headless multiplayer
routine (host plus joiners, `verified=` advancing on every peer).

1. **Basics:** text/address encoding, state hash, local port, and the
   cross-platform fixes (compiler flag, `hypot` replacement, engine sin/cos).
2. **Helpers:** component encoding, `Timeline` save/restore, stable ids.
3. **Session fixes:** not-yet-admitted mode, reserved message types, tolerate
   stranger garbage.
4. **Membership:** joining, admission, ids, roster agreement, leaving, snapshot
   transfer, revisions.
5. **Unexpected disconnects**, written directly in the engine's membership code
   rather than in the goose game first, to avoid writing it twice.
6. **Input sync and rollback**, with the goose game reduced to an adapter over
   `GooseSimulation`.
7. **Short team docs:** what a game implements, and how to keep a simulation
   deterministic.
