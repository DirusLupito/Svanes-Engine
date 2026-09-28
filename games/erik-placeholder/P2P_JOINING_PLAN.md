# P2P joining: agreed direction

## Goal and foundation

Allow players to join Erik's active game world over LAN, initially by contacting
one known peer address. There is one ongoing world, with no lobby or match boundary.

Build on the shared engine's ZeroMQ transport and reliable messaging, plus the
game's fixed-tick input exchange, prediction, rollback, state hashes, and coordinated
roster changes. Peers communicate directly, without an authoritative gameplay host.
Graceful leaving already works. Discovery, internet connectivity, and unexpected
disconnect recovery are separate work.

## Joining approach

Existing peers pause at an agreed, confirmed simulation tick. Transfer the world
at that boundary to the newcomer, reconstruct and verify it locally, then adopt
the new roster and resume together. The contacted peer is an entry point, not an
authority over the world. A snapshot describes the state **before input for tick N**.
Everyone can start fresh rollback history at the agreed boundary.

## State to transfer

- **Session and roster:** boundary tick, roster revision, peer identities and endpoints.
- **Geese:** ownership, position, motion, controller state, timers, and relevant clock state.
- **Enemy:** actual position, health, alive/dead state, and firing timer.
- **Bullets:** stable shot and shooter identities, position, motion, and relevant clock state.
- **Simulation counters:** enough to assign consistent identities to future objects.
- **Animation playback:** animation identity, current sprite-sheet frame, progress toward
  the next frame, and any variable playback settings. Joining should not restart animations.

Enemy movement is gameplay simulation, separate from sprite animation. Its sine-wave
destination follows the shared tick, but its actual position must also be restored
because it moves toward that destination at a limited speed.

## Reconstruction principles

Load assets and recreate the fixed arena locally from a compatible game build.
Transfer dynamic state, not texture handles, pointers, or local registry entity IDs.
Use stable gameplay identities and map them to locally created entities. Collision
ordering must also remain consistent despite different local entity IDs.

Bullets can outlive their shooter, so ownership must survive without a live goose.
Restore animation playback using local assets without advancing simulation time.
Verify compatibility of fixed rules as well as agreement on the transferred state.

## Complete

- `./game` starts a world as peer 1; `--join IPv4:PORT` joins through any member.
- Joins batch into the existing roster pause. Ids come from a shared, only-increasing counter; retired ids are never reused. Player cap of 8, fixed spawn points.
- The contacted member sends the joiner its id, member addresses, and a chunked snapshot; the joiner rebuilds and verifies it against the sponsor's hash. Rules hash, cap, and rejection reasons are checked before admission.
- Engine: stranger/contact packets for pre-admission talk, `AddPeer` at runtime, retired connections released after the delivery timeout.
- Verified across two machines, and with three players joining through a non-host.

## Where to go from here

- **Unexpected disconnects** (next): a crash, closed window, or dropped connection currently stalls or fails the world. Treat it as an unannounced departure.
- **Port collisions**: ZeroMQ lets two processes share a UDP port silently, so copies on one machine need `--port`. Needs a transport that reports or picks a free port.
- **Engine abstraction**: see below.
- **Later**: pause-menu join instead of `--join`, LAN discovery, per-world session ids, and maybe if we decide we care, join passwords.

## Eventual engine abstraction

Prove joining in Erik's game first, then extract reusable mechanisms. Shared joining
and membership operations should let games choose admission and initialization
policy: joining an ongoing world, joining only a lobby, or rejecting joins during a
match. Erik's pause-and-snapshot procedure should not be mandatory for every game.
The concrete engine interfaces remain undecided.

### Rolling back only on inputs that mattered

A misprediction should only cost a rollback when the difference could have changed
the world. Holding fire while moving used to mispredict nearly every tick, because aim
changed constantly but is only read when a shot is attempted. Erik's game now sends aim
relative to the goose, and `Goose::Advance` reports a `GooseIntentUse` saying which
parts of the intent the step depended on. The rollback stores it per tick and asks
`ChangesStep(used, actual, use)` whether a received input could have changed that tick,
without knowing anything about aim itself. Two possible engine shapes:

- **Manual:** the game supplies the comparison and the use report, as Erik's game does
  now. Explicit and cheap, but the game must keep it correct.
- **Automatic:** input fields sit behind accessors that record reads, so reading aim
  marks it as read. Nothing can be forgotten, but every input type needs wrapping.

Neither exists in the engine yet. Revisit once a second input field needs this or rollback moves
into the engine.
