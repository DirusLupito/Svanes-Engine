# P2P generalization: moving the goose game's peer-to-peer stack into the engine

Planning only. Nothing here is implemented yet. The plan covers the audit, the
decisions (each with its options and a recommendation), the target architecture,
and a migration order. Anything marked **Open** is left for us to discuss.

Read alongside `P2P_JOINING_PLAN.md`, which records how joining works today and
what counts as engine-worthy versus goose-specific.

---

## 1. Goal and constraints

**Goal.** Give the team as many reusable P2P tools as we reasonably can, by moving
the generic parts of Erik's goose networking into `svanes`. Leave the
goose-specific parts in the game. The goose game must behave the same after each
step of the move.

**Constraints**, taken from `CLAUDE.md` and the joining plan:

- We are not writing networking for the other games. orbitalEscalation's owner
  builds what his game needs. We extract the pieces both games would share and
  don't add engine features only he would use. When one of his needs costs
  almost nothing to support (a setting instead of a constant), we note it as an
  extension point and don't build it.
- doubleTime's client/server networking is out of scope. Where a piece we extract
  also suits client/server, the plan says so (section 9), but we do no work for it.
- Earn our complexity: no templates, attributes, or abstractions without a
  concrete second use or a clear simplification.
- Data lives with the system that uses it. A new header is justified only when
  it has a `.cpp` with real logic.
- Everything must build on Windows, macOS, and Linux, using `std::` threading
  only.
- Fail loudly. Keep the goose game's habit of stopping with an informative
  status rather than drifting silently.

---

## 2. Audit: what exists today

### 2.1 Engine network layer (`include/svanes/network/`)

| Piece | What it does | Assessment |
|---|---|---|
| `MsgPipe` / `UdpMsgPipe` / `TcpMsgPipe` | Connection-id based datagram and stream transport over ZeroMQ. `UdpMsgPipe` normalizes IPv4 routes and can add remotes. | Solid. Gaps: it doesn't report its local port (the goose game carries it separately in `GooseBoundPipe`), and ZeroMQ `dgram` silently shares a bound port between processes. |
| `MessageWriter` / `MessageReader` | Portable big-endian field serialization. | Solid, and the right base for everything below. Missing: length-prefixed text (the goose game has `WriteGooseText`), and a byte hash. |
| `NetworkSession` | Per-peer reliable, unordered delivery with ids, acks, retries, delivery timeouts, failure reports, retiring, runtime `AddPeer`, and stranger/contact packets. | This is already the generic reliable layer and needs no redesign. Gaps below. |
| `NetworkServer` / `NetworkClient` / `network_replication` | doubleTime's client/server path. Sends structs with `memcpy` (`NetworkMessage::From`). Includes `NetworkEntityMap`, which maps remote entity ids to local ones. | Out of scope. It overlaps with the stable-identity tools proposed below (section 9). |

`NetworkSession` gaps found in the audit:

1. **No way to talk before admission.** A joiner has no peer id and no session, so
   `GooseJoin` hand-encodes contact packets on a bare pipe with a made-up sender
   `GooseUnassignedPeer = 0xFFFFFFFF` (`goose_network.cpp:380`). The joiner also
   has to decode replies itself. An engine API is missing here, and the made-up
   id is an engine-level convention defined in game code.
2. **One stray datagram can crash a peer.** In `NetworkSession::Update`,
   `DecodeSessionPacket` runs before the stranger check (`network_session.cpp`,
   the `found`/`decoded` lines in `Update`). Any datagram that doesn't start
   with the protocol marker throws, and so does any packet from outside the
   roster that is malformed or an acknowledgment. Crashing loudly is right for a
   roster peer's bug. For unsolicited traffic from outside the roster it
   amounts to a denial of service. (Decision D12.)
3. **No reserved message-type range.** Game message types start at 2 in the goose
   game. Once the engine sends its own messages (roster control, snapshots,
   hashes, inputs), the engine's and the game's types must not collide.
   (Decision D4.)

### 2.2 Goose P2P stack: responsibility by responsibility

The three goose network files total about 1,650 lines. Each row is one
responsibility, where it lives now, and where it should go.

| Responsibility | Where now | Generic? | Destination |
|---|---|---|---|
| FNV-1a byte hash | `goose_rollback.cpp:32` **and** `goose_simulation.cpp:388` (duplicated) | Yes | Engine: `HashBytes` in `message_serialization` |
| Length-prefixed text | `WriteGooseText` / `ReadGooseText`, `goose_network.cpp:23` | Yes | Engine: `MessageWriter::WriteText` / `MessageReader::ReadText` |
| Address encode/decode | `WriteGooseAddress` / `ReadGooseAddress`, `goose_network.cpp:42` | Yes | Engine, next to `UdpAddress` |
| Relayed-loopback resolution | `ResolveRelayedAddress`, `goose_network.cpp:57` | Yes | Engine, in the membership layer |
| Bound pipe + port | `GooseBoundPipe`, `BindGoosePipe` | Yes | Engine: `UdpMsgPipe::LocalPort()`, then delete the goose struct |
| Pre-admission join handshake (joiner side) | `GooseJoin`, `goose_network.cpp:357` | Yes | Engine: `PeerJoin` |
| Admission (member side): dedupe, pending/reject replies, resending assignments | `HandleJoinRequests`, `goose_rollback.cpp:574` | Mechanism yes, policy no | Engine mechanism plus a game admission callback |
| Assignment message (id, sponsor, revision, member endpoints) | `GooseAssignment`, `goose_network.hpp` | Yes | Engine |
| Shared never-reused id counter | `next_peer_id` in `GooseRollback` | Yes | Engine membership |
| Roster revision tagging + next-revision buffering | `GooseNetwork::Update` / `Broadcast`, `goose_network.cpp:119, 193` | Yes | Engine membership |
| Applying a roster change to the session (retire/add, keep the sorted roster) | `ApplyRosterChange`, `goose_network.cpp:291` | Yes | Engine membership |
| Chunked snapshot transfer | `SendSnapshot` / `ReadSnapshotChunk` / `TakeSnapshot` | Yes | Engine, as a general byte-blob transfer |
| Coordinated roster pause (Stop → boundary → Prepared → commit) | `BeginRosterPause` / `ReceiveRosterMessage` / `UpdateRosterPause`, `goose_rollback.cpp:556–818` | Mostly. The agreement is generic; "pause at a tick" is one policy for it | Engine membership, with the boundary supplied by the game |
| Leaving | `RequestLeave` / `CanClose` / departed handling | Yes | Engine membership |
| Fixed-tick input exchange, per-player input streams, confirmed tick | `ReceiveMessages` / `RecordInput` / `ConfirmedTick` | Yes, if inputs are bytes | Engine input sync |
| Prediction (repeat last input, clear one-shot presses) | `InputFor`, `goose_rollback.cpp:304` | Repeating is generic; clearing `dash` is the game's | Engine default plus a game override |
| Misprediction check with "which parts were read" | `ChangesStep` + `GooseIntentUse`, `RecordInput` | The mechanism is generic; the comparison belongs to the game | Engine stores a per-tick use mask; the game compares |
| Snapshot history, restore, and replay | `history`, `AdvanceTick`, `PruneHistory`, the correction block in `Update` | Yes | Engine input sync |
| Pacing against peer progress | `PacingStepTics`, `goose_rollback.cpp:475` | Yes | Engine input sync |
| Fixed-step accumulator with a backlog cap and steps per frame | `pending_tics` / `MaximumBacklog` / `StepsPerFrame` | Yes | Engine input sync (possibly a standalone helper, D10) |
| Periodic state-hash checkpoints | `ReceiveStateHash` / `CheckStateHashes` | Yes | Engine input sync |
| Rules/compatibility hash | `RulesHash`, `goose_rollback.cpp:115` | The envelope is generic; the contents belong to the game | The engine combines its own protocol and settings with a game-supplied hash |
| Join snapshot envelope (next id, roster, world, hash) + rebuild-and-verify | `EncodeJoinSnapshot`, the second `GooseRollback` constructor | Envelope yes, world no | Engine envelope; the game encodes and restores the world |
| Local input sampling, double-tap latching, aim relative to the goose | `LocalControls`, `TakeLocalInput`, the input block in `Update` | No | Stays in the goose game |
| `GooseIntent` and its encoding and validation | `EncodeInput`, the input decode in `ReceiveMessages` | No | Stays in the goose game |
| World capture, encode, decode, and restore | `GooseSimulation` | No, but it is built from generic component codecs | Stays in the goose game; engine component codecs replace the helpers (D8) |
| Spawn points, player cap value, arena | `goose_simulation.cpp`, `GooseMaxPlayers` | No | Stays in the goose game (the cap becomes a setting value) |
| Status strings and diagnostics | `Status` / `Diagnostics` | Partly | The engine reports a state enum plus detail; the game formats it (D11) |

About 70–75% of `goose_rollback` and `goose_network` is generic. After the
migration the goose game keeps `GooseSimulation`, its input type, its input
sampling, and a small adapter that implements the engine's simulation interface
(section 6).

### 2.3 orbitalEscalation audit

The owner cares about P2P, so this section records what his game does today and
what that implies for us. It is not a plan to network his game.

What it does:

- A single-player square with thrusters, a planet with a `PointAttractor2D`
  gravity field, **120 NPC bodies** (boxes, circles, triangles) in orbit,
  all-pairs collisions (`DetectEntityCollisions`, O(n²), about 7,000 pairs per
  step), bodies destroyed when they hit the boundary walls, and cosmetic
  collision "flashes".
- It uses the engine's automatic simulation (`automatic_simulation = true`) and
  reads input in `PhysicsUpdate` from `physics.input`, the local
  `InputManager`.
- Pausing (the P key) pauses a parent **gameplay timeline** that every body's
  `Timeline` is parented to.
- It uses `std::thread::hardware_concurrency()` workers for kinematics.

What would block deterministic P2P (lockstep or rollback):

1. **Nondeterministic setup.** NPCs are seeded from `std::random_device`, and
   `std::uniform_real_distribution` / `std::uniform_int_distribution` are
   implementation-defined, so their output differs across MSVC, libstdc++, and
   libc++ even with a fixed seed. Peers would start in different worlds.
   Possible fixes: send the initial world in a snapshot, or share a seed and use
   a portable RNG (D9).
2. **Input comes from the local `InputManager` inside the physics step.** Under
   synchronized simulation, a step has to consume the synchronized inputs of
   every player.
3. **Pause is a local timeline toggle.** It would have to become an input
   ("toggle pause" in some player's tick input) or a roster-style agreement.
4. **Parented, pausable timelines.** The goose game's timeline encoding only
   supports root, running, unscaled timelines (`WriteTimeline`,
   `goose_simulation.cpp:77`, which throws otherwise). orbitalEscalation needs
   the full `Timeline` state, including `incomplete_progress`, `paused`,
   `tic_size`, and the parent mapped to a stable identity (D8).
5. **Cosmetic entities are created inside the step.** Flashes are spawned in
   `PhysicsUpdate`. Under rollback they would be created again on every replay.
   Under plain lockstep that is harmless.
6. **Simulation cost versus rollback.** Replaying up to 30 ticks of 7,000-pair
   SAT in one frame is a large spike. Rollback suits the goose game (few
   entities, cheap steps). orbitalEscalation's world is dominated by input-free
   NPC physics, which argues for **delay-based lockstep**: every tick is
   simulated once, when all inputs have arrived, and local input is delayed a
   few ticks to hide latency.

Parts of the engine it would reuse directly: join admission, shared ids,
roster changes and leaving, chunked world transfer, state-hash desync checks,
the component codecs, the stable identity helpers, and the input-sync layer
with prediction turned off (D6). This matches the joining plan's intent: shared
pieces, with his game-specific policy left to him.

### 2.4 Cross-cutting findings: determinism

These affect any deterministic P2P game, the goose game included. The engine
should at least document them, and in places provide a tool.

1. **`Registry::ForEach` order is unspecified.** Components live in a
   `std::unordered_map<Entity, …>`. Iteration order depends on entity ids,
   insertion and erase history, and the standard library. A joiner that rebuilt
   the world has different entity ids. The goose game works around this by hand:
   `OrderedBullets` sorts by shot id, and `BulletTargets` sorts arena solids by
   entity id (`goose_simulation.cpp:453`). That second sort is correct only
   because every process creates the arena first and in the same order, which
   is fragile. Engine systems that iterate via `ForEach` are safe only where the
   per-entity work is independent. `AdvanceKinematics` is fine.
   `EvaluateAttractors` sums floats in attractor order, which matters once there
   are two or more attractors. (D7.)
2. **Floating-point consistency across platforms.** Lockstep and rollback
   require bit-identical results on every peer.
   - No floating-point flags are set in `CMakeLists.txt`. Clang on ARM64
     (Apple Silicon) contracts `a*b+c` into FMA by default
     (`-ffp-contract=on`). GCC in strict ISO mode (our `CXX_EXTENSIONS OFF`) and
     MSVC `/fp:precise` do not. **A Mac peer would likely desync from a Linux
     or Windows peer.**
   - `std::sin`, `std::cos`, `std::hypot`, and `std::pow` differ between glibc,
     the MSVC CRT, and Apple's libm. The goose enemy path uses `std::sin`
     (`goose_simulation.cpp:191`), and orbitalEscalation's field uses
     `std::hypot`.
   - The state-hash check will catch a desync, but only after the fact. (D13.)
3. **`std::` distributions aren't portable** (see 2.3, point 1).
4. **Simulation versus presentation.** Anything created during a step and not
   captured in the snapshot (flashes, sounds) repeats on replay. The goose game
   avoids this because it has no such effects yet.

### 2.5 Other findings

- **Traffic shape.** Each tick's input is one reliable message per peer, so
  every message is acknowledged separately. With 8 players at 100 ticks/s that
  is about 700 data packets plus 700 acks per second per peer. Fine on a LAN.
  `max_pending_per_peer = 256` means a stall of about 2.5 s fills the window
  (the "Waiting for outgoing message capacity" state). GGPO-style redundant
  unreliable input packets would be leaner. That isn't needed now and is noted
  as future work (section 10).
- **Timeouts and limits are scattered constants.** Prediction 30, history 256,
  hash interval 100, max unchecked 200, 8 steps per frame, pacing lead 3,
  backlog of 25 steps, roster pause stall 15 s, assignment retention 15 s, join
  timeout 15 s, join retry 250 ms, snapshot chunk 1024 B, at most 4096 chunks,
  4096 queued messages. All of these become engine settings structs.
- **Manual stepping duplicates the application loop.** `GooseSimulation::Step`
  advances timelines by hand, calls `AdvanceKinematics`, then
  `AdvanceSpriteAnimations`. `application.cpp` does the same when
  `automatic_simulation` is on. Every game that steps manually (any
  synchronized game) would copy this. (D10.)
- **Unexpected disconnects** are next on the goose roadmap and touch exactly the
  layer we are extracting (section 8, phase 4).

---

## 3. A short primer on keeping peers in sync

Erik asked for this. It explains why one engine layer can serve both games and
why it shouldn't force one model on either.

| Model | What is sent | Needs determinism? | Cost profile | Fits |
|---|---|---|---|---|
| **Deterministic lockstep (delay-based)** | Inputs only | Yes, bit-exact | Every tick simulated exactly once. Input latency is roughly the round trip, hidden by an input delay. A slow peer stalls everyone. | Many entities, cheap inputs: **orbitalEscalation** |
| **Rollback (predictive lockstep)** | Inputs only | Yes, bit-exact | Local input feels instant. Mispredictions restore a snapshot and replay, so cost spikes with replay depth times step cost. | Few entities, twitchy controls: **goose game** (today) |
| **Snapshot / state sync** | Entity state | No | More bandwidth. Peers show slightly different pasts; interpolation smooths it. | Physics-heavy games that give up determinism |
| **Distributed authority** | Each peer sends the state of entities it owns | No | Conflicts at ownership boundaries, such as collisions between bodies owned by different peers. | An alternative for orbitalEscalation if cross-platform determinism proves too hard |
| **Client/server** | Inputs up, state down | No | One authority. | doubleTime (out of scope) |

Rollback with the prediction window set to zero *is* delay-based lockstep: the
game never runs ahead of confirmed input, so it never rolls back. That is why
the engine's input-sync layer can serve both deterministic models through
settings (D6).

Everything below the input-sync layer (admission, ids, roster changes, leaving,
blob transfer, hashes, codecs, stable ids) is useful to **all** P2P models,
including state sync and distributed authority. That is the part to prioritize.

---

## 4. Target architecture

```
 ┌───────────────────────────────────────────────────────────────────────┐
 │ Game (goose / orbital)                                                │
 │  • world simulation: Step, Save/Load bytes, Add/RemovePlayer          │
 │  • input type + encoding, local sampling/latching                     │
 │  • admission policy, rules hash, player cap value, presentation       │
 └───────────────▲───────────────────────────────▲───────────────────────┘
                 │ implements SyncedSimulation   │ uses directly (non-lockstep games)
 ┌───────────────┴───────────────┐               │
 │ L3  InputSync (optional)      │               │
 │  tick input streams, predict, │               │
 │  snapshot history, rollback,  │               │
 │  pacing, fixed-step clock,    │               │
 │  state-hash checkpoints,      │               │
 │  join-snapshot envelope       │               │
 └───────────────▲───────────────┘               │
 ┌───────────────┴───────────────────────────────┴───────────────────────┐
 │ L2  PeerGroup (membership)          PeerJoin (joiner handshake)       │
 │  roster + shared id counter, revision tagging, admission mechanism,   │
 │  two-phase roster change with game-supplied boundary + state hash,    │
 │  leaving, blob transfer (chunked), relayed-address resolution         │
 └───────────────▲───────────────────────────────────────────────────────┘
 ┌───────────────┴───────────────────────────────────────────────────────┐
 │ L1  NetworkSession (exists)  + unadmitted mode, reserved types,       │
 │                                safe handling of junk from strangers   │
 └───────────────▲───────────────────────────────────────────────────────┘
 ┌───────────────┴───────────────────────────────────────────────────────┐
 │ L0  UdpMsgPipe (+LocalPort), MessageWriter/Reader (+Text, HashBytes), │
 │     UdpAddress codec                                                  │
 └───────────────────────────────────────────────────────────────────────┘

 Side utilities (usable by any layer or game):
   component codecs (Transform, Kinematic2D, Timeline, SpriteAnimation, Sprite source,
   Color, Geometry2D) · TimelineState · StableId + deterministic ordering ·
   StepSimulation helper · (later) portable RNG
```

Rules of the layering:

- Each layer works without the one above it. orbitalEscalation could use
  `PeerGroup` alone if he goes with state sync.
- Networking stays **game-driven**: the game calls `Update` on these objects
  from `IGame::Update`, as the goose game does now. `Application` does not change
  (D2).
- Everything runs on the main thread, pumped once per rendered frame, including
  while paused.

### 4.1 L0 additions

- `MessageWriter::WriteText(std::string_view)` / `MessageReader::ReadText()`, a
  16-bit length followed by the bytes. This moves `WriteGooseText` as is.
- `std::uint64_t HashBytes(std::span<const std::byte>)` (FNV-1a) in
  `message_serialization.hpp`. It replaces both goose copies. It is documented
  as a desync detector, not a cryptographic hash.
- `WriteUdpAddress` / `ReadUdpAddress`, declared in `udp_msg_pipe.hpp` (the
  owner of `UdpAddress`).
- `UdpMsgPipe::LocalPort()`. `GooseBoundPipe` goes away.

### 4.2 L1 `NetworkSession` additions

- **Unadmitted mode.** Construct with `local_peer = UnassignedPeer` (an engine
  constant, replacing `GooseUnassignedPeer`) and an empty roster. `Send` and
  `Broadcast` throw in this mode. `SendStranger` and `ReceiveStranger` work.
  `AssignLocalPeer(PeerId)` switches to a normal session once admitted. The
  pipe then never leaves the session, and `PeerJoin` stops hand-encoding
  packets. (D3.)
- **Reserved message types.** For example, engine protocol messages use
  `0xF000–0xFFFF` and games get everything below. The session rejects game sends
  in the engine range. (D4.)
- **Junk from strangers** is dropped and logged once per connection instead of
  throwing. Malformed data from a roster peer still throws. (D12.)

### 4.3 L2 `PeerGroup` and `PeerJoin` (new: `include/svanes/network/peer_group.hpp` + `.cpp`)

This extracts `GooseNetwork` and the membership half of `GooseRollback`.

**Owns:** the `NetworkSession`, the sorted roster (`std::vector<PeerId>`, local
peer included), member addresses, the roster revision, and the shared
`next_peer_id` counter, which only increases and never reuses an id.

**Revision-tagged messaging.** `Broadcast(type, payload)` and
`Send(peer, type, payload)` prefix the current revision. `Receive` delivers only
current-revision messages, buffers messages from the next revision, drops older
ones, and throws for anything further ahead. This is `GooseNetwork` behavior
moved as is.

**Admission (member side).** `PeerGroup::Update` reads join requests from
strangers, answers repeats with Pending or a re-sent assignment (retained for a
configurable time), and asks the game once per new candidate:

```cpp
struct JoinCandidate { ConnectionId connection; UdpAddress address; NetworkMessage request; };
enum class AdmissionVerdict : std::uint8_t { Accept, Reject };
struct AdmissionDecision { AdmissionVerdict verdict; std::string reason; };
// supplied by the game; engine checks compatibility hash, duplicate address, cap, and "local is leaving" first
std::function<AdmissionDecision(const JoinCandidate&)> admission_policy;
```

The engine applies the generic rejections itself: incompatible build, address
already a member, group full (`max_members`), and contacted member leaving. The
policy covers everything game-specific: "match in progress", "lobby only",
passwords, and so on. Accepted candidates wait for the next roster change.

**Roster change agreement.** This is the general form of the goose "roster
pause". It is two-phase and leaderless, as in the goose game today:

1. Something begins a change: a local leave request, a queued join, or a
   remote Stop message. Each member **freezes** and broadcasts
   `Stop{boundary_proposal, leaving, admitted addresses}`. The boundary proposal
   is a `std::uint64_t` the game supplies. For tick-based games it is the first
   tick the member hasn't generated input for. A game without ticks passes 0.
2. When every current member's Stop has arrived, the agreed boundary is the
   maximum proposal. `PeerGroup` reports `BoundaryAgreed{boundary}` and waits for
   the game to call `ReachedBoundary(state_hash)`.
3. Each member broadcasts
   `Prepared{boundary, state_hash, roster_hash}`. The roster hash covers the
   ordered stops, the revision, and `next_peer_id`. Once all Prepared messages
   arrive and match, the change **commits**: ids are assigned in sorted
   (sponsor id, request order) order, capped by `max_members`, and the session
   roster is updated (retire and add). The revision increments, and buffered
   next-revision messages are released.
4. The game receives `RosterCommitted{departed, joined (id + sponsor), rejected}`.
   It adds and removes players. For each joiner this member sponsored,
   `PeerGroup` sends the assignment and then any blob the game hands it (the
   world snapshot).
5. A stall timeout (default 15 s) fails the group with a clear status.

This keeps the goose semantics exactly. It also means the game decides what
"the boundary" and "the state hash" are: the goose game uses a tick and a world
hash, and a lobby-only game could use 0 and 0.

**Leaving.** `RequestLeave()` starts a roster change with `leaving = true`.
After commit the local peer is departed: every remote peer is retired, and
`CanClose()` becomes true once outgoing traffic drains. A failed session sets
`force_close` behavior as in the goose game.

**Blob transfer.** `SendBlob(peer, NetworkMessage)` / `TakeBlob()`, which is the
goose chunking made general. Chunks carry `(index, count)`. The receiver
validates consistency and reassembles once complete. The only current use is the
join snapshot, so it deliberately has **one** incoming blob slot and no
transfer ids. We add ids only when a second use appears (D5).

**PeerJoin (joiner side).** It replaces `GooseJoin`. It is constructed with the
entry address, a local port, and the compatibility hash (plus an optional
game-defined request payload). It retries on an interval, times out, and
reports Pending, Rejected with a reason, or Assigned. `Admit()` returns a
`PeerGroup` built from the assignment, with relayed loopback addresses resolved
against the sponsor's host. The group is then waiting for the sponsor's blob.

**Peer failure.** `NetworkSession` failures show up as `PeerGroup` failure
status. Disconnect recovery plugs in here later (section 8, phase 4).

### 4.4 L3 `InputSync` (new: `include/svanes/network/input_sync.hpp` + `.cpp`)

This extracts the tick and input half of `GooseRollback`. It is optional; a game
that doesn't use deterministic lockstep ignores this layer.

**The game implements one interface:**

```cpp
struct TickInput { PeerId peer; const NetworkMessage& input; };   // roster order

class SyncedSimulation {
public:
    virtual ~SyncedSimulation() = default;
    virtual NetworkMessage SaveState() const = 0;                 // portable bytes: the world before the next tick
    virtual void LoadState(const NetworkMessage& state) = 0;
    virtual std::vector<std::uint32_t> Step(std::uint64_t tick, std::span<const TickInput> inputs) = 0; // per-player use masks
    virtual void AddPlayer(PeerId peer) = 0;
    virtual void RemovePlayer(PeerId peer) = 0;
    virtual NetworkMessage SampleLocalInput() = 0;                // called once per generated tick
    virtual NetworkMessage NeutralInput() const = 0;             // used while finishing to a roster boundary
    virtual NetworkMessage PredictInput(const NetworkMessage& last) const { return last; }
    virtual bool InputChangesStep(const NetworkMessage& used, const NetworkMessage& actual,
                                  std::uint32_t use_mask) const { return used.bytes != actual.bytes; }
    virtual void ValidateInput(const NetworkMessage& input) const = 0; // throws for malformed peer input
};
```

(These are sketches of the shape, not final signatures. Doc comments per
`CLAUDE.md` come with the implementation.)

**Engine side:** it owns the tick counter, per-player input streams keyed by
tick (as bytes), the confirmed tick, a history of `SaveState()` bytes keyed by
tick plus the inputs and use masks applied, correction detection (the earliest
tick where `InputChangesStep` is true), restore and replay, pacing, the
fixed-step accumulator, state-hash checkpoints (`HashBytes` of the stored state
bytes at every `hash_interval` confirmed boundary), "peer changed its input" and
"peer changed its hash" detection (byte equality), and all the waiting and
failure states. It drives `PeerGroup`'s roster change. When it reaches the
agreed boundary it finishes with neutral input, hashes the state there, and
calls `ReachedBoundary`. On commit it calls `Add/RemovePlayer`, resets the input
streams, and, when sponsoring, sends the **join envelope**:

```
next_peer_id · roster · tick · state bytes · HashBytes(state bytes)
```

The joiner-side constructor reads the envelope, calls `AddPlayer` for each
member, then `LoadState`. It re-saves and checks that the hash matches, as
`GooseRollback`'s second constructor does now.

**Settings** (`InputSyncSettings`): `step_tics`, `max_prediction_ticks` (30 for
goose; 0 gives pure lockstep), `history_ticks`, `hash_interval_ticks`,
`max_unchecked_ticks`, `max_steps_per_frame`, `pacing_lead_ticks`,
`max_backlog_steps`, `max_members`. Each is validated against the others. For
example, history must exceed prediction plus the hash interval, which the goose
game currently only guarantees by choosing the numbers.

**Compatibility hash** = `HashBytes(engine protocol version ‖ InputSyncSettings
‖ game rules hash)`. It is sent in the join request and checked by the engine.

### 4.5 Side utilities

- **Component codecs** (`include/svanes/network/component_serialization.hpp` +
  `.cpp`): `Write`/`Read` pairs for `Transform`, `Kinematic2D`, `Timeline` (via
  `TimelineState`), `SpriteAnimation`, the `Sprite` source rectangle, `Color`,
  and `Geometry2D` (the variant: rectangle, circle, triangle, convex polygon,
  composite). These come straight from the goose helpers
  (`goose_simulation.cpp:19–104`) plus geometry, which orbitalEscalation needs
  because its body shapes vary. Texture handles and local entity ids are never
  encoded. (D8.)
- **`TimelineState`** in `timeline_system.hpp`: a plain struct with the full
  state (parent as an optional *stable* id supplied by the caller, tic size,
  paused, total, delta, incomplete progress) plus `Timeline::Capture()` /
  `Timeline::Restore(const TimelineState&)`. This replaces the goose
  "advance twice" rebuild and lifts the root, running, unscaled restriction.
- **Stable identity** (`include/svanes/network/stable_identity.hpp` + `.cpp`): a
  `StableId { std::uint64_t value; }` component, a deterministic allocator whose
  counter belongs in the game's snapshot, a
  `std::vector<Entity> SortedByStableId<Components...>(world)` helper
  (templated because `ForEach` is), and a `StableId → Entity` lookup rebuilt
  after restore. The goose game's shot ids and `owner_key` become the first
  user. (D7.)
- **`StepSimulation(world, gravity, driver, step_tics, after_step)`** in
  `physics_system.hpp`: the timeline, physics, callback, and animation sequence
  `application.cpp` runs, exposed so games that step manually don't copy it.
  The application loop calls it too. (D10.)
- **Portable RNG** is not built now; it is listed as an extension point (D9).

---

## 5. Decisions

Each decision lists its options and a recommendation. Recommendations are
what I'd do; we can revisit any of them.

**D1. Extract now, or prove disconnects in the game first?**
- (a) Extract everything, then implement disconnects in the engine.
- (b) Implement disconnects in the goose game first, then extract.
- (c) Extract L0, L1, and the side utilities first, then L2 and L3, and write
  disconnects directly in the engine's `PeerGroup`.
- **Recommend (c).** Disconnect handling lives almost entirely in membership and
  roster-change code. Writing it in `GooseRollback` and then moving it means
  writing it twice. The joining plan's "prove in the game first" was about
  unknown design. The membership design is proven now, and disconnect recovery
  is an extension of the same agreement (section 8). **Open:** whether Erik
  prefers the proven-in-game route anyway.

**D2. Who drives networking: the game or the `Application` loop?**
- (a) The game calls `PeerGroup::Update` / `InputSync::Update` from
  `IGame::Update` (as now).
- (b) `Application` owns a network hook and pumps it.
- **Recommend (a).** No changes to `Application` or `IGame`, and games opt in
  without touching the loop. Networking must also pump while the game isn't
  simulating, which the game controls.

**D3. How does a joiner talk before it has an id?**
- (a) Unadmitted `NetworkSession` mode plus `AssignLocalPeer`.
- (b) Free functions `SendContact` / `DecodeContact` over a bare pipe (formalize
  what `GooseJoin` does).
- (c) Keep the goose approach.
- **Recommend (a).** The pipe stays owned by one object for its whole life, and
  the stranger machinery already exists. (b) is fine but duplicates packet
  handling.

**D4. Message-type namespace.**
- (a) The engine reserves a high range (`0xF000+`), and games use the rest.
- (b) Two-level types: an engine or game channel byte plus a type.
- **Recommend (a).** One comparison, no wire change, and game types stay small
  numbers.

**D5. Blob transfer: general or join-only?**
- (a) One incoming slot with no transfer ids, as today.
- (b) Transfer ids, several concurrent transfers, named kinds.
- **Recommend (a)** until a second use appears (earn the complexity). The
  sender side already supports several joiners at once.

**D6. How do games supply inputs and state to the input-sync layer?**
- (a) Bytes plus a virtual interface (`SyncedSimulation` above). The engine
  stores inputs and states as `NetworkMessage`, compares by bytes by default,
  and hashes the saved bytes.
- (b) A class template `InputSync<Game>` over typed `Input` / `Snapshot`.
- (c) The engine stores only inputs, and the game owns snapshot storage
  (GGPO-style `Save(tick)` / `Load(tick)` / `Discard(before)` callbacks).
- **Recommend (a).** One encoding serves history, hashes, and join transfer, and
  "we hash exactly what a joiner receives" becomes true by construction. That
  principle is already in `GooseSimulation::Hash`. Nothing lands in headers as
  template code. The cost is encoding the world every tick. For the goose game
  that is well under 1 KB; for orbitalEscalation about 10 KB (120 bodies at
  about 80 B). Both are cheap next to one physics step. Fall back to (c) if
  profiling ever shows encoding matters.
- Note: `max_prediction_ticks = 0` turns this into delay-based lockstep for
  free. An **input delay** setting (local input for tick t is scheduled at
  t + d) is the missing half of good lockstep. It is an **extension point, not
  built**, because only orbitalEscalation would use it.

**D7. Deterministic iteration: stable ids or an ordered registry?**
- (a) A `StableId` component plus sorted-iteration helpers. `ForEach` stays
  unordered.
- (b) Make `Registry` iterate in entity-id order (`std::map`).
- (c) Document the problem only.
- **Recommend (a).** (b) slows every game and still isn't enough, because a
  joiner's entity ids differ, so their order can differ too. Stable ids fix the
  real problem and also give snapshots something portable to reference (bullet
  owners, timeline parents).

**D8. Component serialization: where and how much?**
- (a) Engine codecs for engine components, in one new header, with the full
  `Timeline` state added to the engine.
- (b) Leave serialization to each game.
- **Recommend (a).** Both P2P games need the same Transform, Kinematic2D, and
  Timeline encoders, and doubleTime's `memcpy` structs would benefit later. The
  `Timeline` change is needed regardless; it can't be encoded properly from
  outside the class.

**D9. Portable random numbers.**
- (a) Engine `DeterministicRandom` (such as PCG32) with its own
  uniform-float and uniform-int functions and state that can be snapshotted.
- (b) Not in the engine. Games that need it write it or send their initial world
  in the snapshot.
- **Recommend (b) for now, listed as an extension point.** The goose game
  doesn't use randomness in simulation. orbitalEscalation can avoid it entirely
  by transferring its world in the join snapshot. Build (a) when a second game
  needs randomness *during* simulation.

**D10. `StepSimulation` helper.**
- **Recommend building it.** It is a small, behavior-preserving extraction from
  `application.cpp`. The goose game adopts it only if it fits: the goose game
  currently advances only goose and bullet timelines (not the orb's) and orders
  the bullet steps itself. **Open:** whether the goose game adopts it or keeps
  its explicit stepping.

**D11. Status reporting.**
- (a) The engine returns a `SyncState` enum (Playing, WaitingForInputs,
  WaitingForHashes, WaitingForCapacity, ChangingRoster, Departed, Failed) plus
  detail such as the failing peer, the tick, and the local and remote hash.
  The game formats text.
- (b) The engine returns strings, as the goose game does.
- **Recommend (a)** plus a `Describe()` convenience that returns the current
  goose wording, so the console output stays the same and our headless test
  greps (`verified=`) keep working.

**D12. Malformed or unexpected traffic.**
- **Recommend:** from roster peers, throw (it's our bug, so crash loudly, per the
  style guide). From strangers, drop and log once per connection. A stray
  datagram on the port must not crash a game.

**D13. Cross-platform floating point.**
- (a) Add `-ffp-contract=off` (GCC/Clang) and make sure `/fp:precise` (MSVC) is
  set for the engine and games. Document that `std::` transcendental functions
  may differ across platforms, and optionally mix a platform or toolchain
  fingerprint into the compatibility hash so mismatched peers are *rejected at
  join* instead of desyncing later.
- (b) Leave it and rely on the state-hash check.
- **Recommend (a) without the fingerprint first**, then test Linux↔Windows and
  Linux↔Mac with the goose game. If they desync (likely because of `std::sin`
  in the enemy path), either add the fingerprint (honest: "same platform only")
  or replace the few transcendental calls in simulation. **Open.** This affects
  the class demo if players use mixed OSes.

**D14. Naming.**
- **Recommend** `PeerGroup` (membership), `PeerJoin` (joiner handshake),
  `InputSync` + `SyncedSimulation` (lockstep and rollback),
  `component_serialization`, and `StableId`. **Open:** the team may prefer
  `P2PSession`, `LockstepSession`, `RollbackSession`, or similar.

---

## 6. What stays in the goose game

After the migration the goose game keeps:

- `GooseSimulation` (world, spawn points, enemy path, bullets), with its encode
  and decode rewritten on engine codecs and its shots using `StableId`.
- A `GooseSync` adapter, roughly 150–250 lines, implementing
  `SyncedSimulation`:
  `SaveState`/`LoadState` → `Capture`+`Encode` / `Decode`+`Restore`;
  `Step` → decode `GooseIntent`s, call `GooseSimulation::Step`, return the
  `GooseIntentUse` masks; `PredictInput` clears `dash`; `InputChangesStep` →
  `ChangesStep`; `SampleLocalInput` → `TakeLocalInput` (latching and double-tap
  stay here); `ValidateInput` → the current range checks.
- `GooseIntent`, its encoding, `GooseIntentUse`, and `ChangesStep`.
- The admission policy (none beyond the engine defaults; `max_members = 8`), the
  rules hash contents (gravity, step size, protocol version), ports, and
  command-line handling.
- `ErikGame`, which wires `PeerJoin` → `PeerGroup` → `InputSync` together much
  as it wires `GooseJoin` → `GooseNetwork` → `GooseRollback` now.

`goose_network.*` goes away entirely. `goose_rollback.*` becomes the adapter.
Net result: about 1,250 fewer game lines and about 1,100–1,300 new engine lines.

---

## 7. What orbitalEscalation's owner would get

Guidance for him, not work for us:

- **Any model:** `PeerJoin`/`PeerGroup` for joining through any member,
  never-reused ids, rejections with reasons, leaving, and roster changes;
  `SendBlob` for the initial world; the component codecs including
  `Geometry2D` and full `Timeline` state; `StableId` for his NPCs.
- **If he goes deterministic** (recommended in 2.3): `InputSync` with
  `max_prediction_ticks` at 0 or a small value, desync detection for free, and
  the join envelope. His tasks: a `SyncedSimulation` adapter; moving input out
  of `PhysicsUpdate` into `Step`; making pause an input; stable-id-ordered
  iteration wherever order matters; either transferring the initial world or a
  portable RNG; and settling D13 before testing across OSes. Input delay (D6)
  is the engine feature he would most likely ask for.
- **If he goes state sync or distributed authority:** `PeerGroup`, blobs,
  codecs, and `StableId` still apply. `InputSync` doesn't. He would write his
  own state broadcast over `PeerGroup::Broadcast`, probably unreliable, which
  `PeerGroup` doesn't offer yet (section 10).

---

## 8. Migration phases

Every phase leaves the goose game working. Each is verified headlessly per the
existing routine: offscreen SDL, `stdbuf -oL`, a host plus two joiners (one
joining through a non-host, one over the LAN IP). Pass criteria: "Joined at
tick N" appears, `verified=` advances in steps of 100 on every peer with the
same roster size, Escape-leave works, and there are no new warnings. Build with
`just build erik` (the full build fails in doubleTime on Linux, as known). Run
`git clang-format` each phase. Nothing moves in one giant commit.

**Phase 0: L0 utilities (small, pure moves).**
`WriteText`/`ReadText`, `HashBytes`, the `UdpAddress` codec, and
`UdpMsgPipe::LocalPort`. The goose game switches over, and `GooseBoundPipe` and
the duplicate hashes go away. *Verify:* identical rules hash and state hashes
(the bytes are unchanged).

**Phase 1: Side utilities.**
`TimelineState` plus `Capture`/`Restore`, the component codecs, `StableId`, and
optionally `StepSimulation` (D10). The goose encode and decode switch over.
*Verify:* join and hash checks pass. The wire format may change here, so bump
`ProtocolVersion`.

**Phase 2: L1 session hardening.**
Unadmitted mode plus `AssignLocalPeer`, the reserved type range, and junk
tolerance for strangers. `GooseJoin` stops hand-encoding packets.
*Verify:* join and leave. Also send a garbage datagram to the port (for example
`printf 'x' | nc -u -w0 127.0.0.1 45000`) and confirm it is logged, not fatal.

**Phase 3: L2 `PeerGroup` / `PeerJoin`.**
Move `GooseNetwork`, admission, the roster agreement, leaving, and blobs.
`GooseRollback` keeps inputs and rollback but drives the roster change through
`PeerGroup`'s boundary and hash hooks. *Verify:* the full join matrix,
rejection reasons (incompatible hash via a modified build, full group with
`max_members = 2`), simultaneous joins through two different members, and a
leave during a join.

**Phase 4: Unexpected disconnects, in the engine** (if D1 = (c)).
Treat a `ReceivePeerFailure` as an unannounced leave: survivors start a roster
change marking the failed peer as leaving, with a boundary proposal equal to
**the highest input tick any survivor holds for it**, so no one simulates past
input that some survivor lacks. Survivors must relay the failed peer's missing
inputs to each other, which needs a new engine message. A failure during the
pause restarts the pause without that peer. *This is the one genuinely new
design in the plan*, and it deserves its own short plan before implementation.

**Phase 5: L3 `InputSync`.**
Move the tick streams, prediction, history, rollback, pacing, hashes, and the
join envelope into the engine. Write `GooseSync`, and delete the remainder of
`GooseRollback`. *Verify:* everything above, plus hand-played sessions where
rollback count and depth resemble today's `Diagnostics` numbers, with no
desyncs over several minutes of firing and dashing.

**Phase 6: Docs for the team.**
A short `src/network/P2P.md` (in the style of `entity_components.md`) covering
the layers, what a game implements, the determinism checklist (section 2.4), and
a minimal wiring example taken from the goose game.

---

## 9. Pieces client/server could also use (no work planned)

Noted only because the brief asked. doubleTime could adopt these later if its
owner wants them:

- `MessageWriter`/`Reader` plus the component codecs, replacing `memcpy`
  structs (`NetworkMessage::From`), which aren't portable across compilers or
  endianness.
- `NetworkSession` reliability. The server is just "peer 1".
- `PeerJoin`-style admission with a compatibility hash and rejection reasons,
  where "join through a sponsor" becomes "join through the server".
- Blob transfer for the initial world, and `StableId` in place of
  `NetworkEntityMap`'s raw server entity ids.

---

## 10. Non-goals and future work

**Not doing:**

- LAN discovery, NAT traversal, and internet play.
- Join passwords and per-world session ids (the goose `GooseSession = 1` stays
  a game constant for now).
- An unreliable broadcast channel, redundant unreliable input packets
  (GGPO-style), or input batching.
- Input delay, portable RNG, multiple concurrent blob transfers.
- Automatic use-tracking of inputs (the joining plan's "automatic" option). The
  manual mask stays, per the joining plan's own recommendation to revisit only
  when a second input field needs it.
- Moving networking to a worker thread.
- Fixing ZeroMQ's silent port sharing (a real transport issue, but separate
  work: bind-failure detection or picking a free port needs a different socket
  option or a probe).

**Open questions for our discussion:**

1. D1: extract before or after disconnect handling?
2. D6: are we happy encoding world bytes every tick, or do we want
   game-owned snapshot storage from the start?
3. D10: does the goose game adopt `StepSimulation`?
4. D13: cross-platform policy. Do we fix, fingerprint, or document?
5. D14: names.
6. Should we tell orbitalEscalation's owner about sections 2.3, 2.4, and 7 now,
   so his early choices (random setup, input in `PhysicsUpdate`, pause) don't
   close off deterministic P2P?
