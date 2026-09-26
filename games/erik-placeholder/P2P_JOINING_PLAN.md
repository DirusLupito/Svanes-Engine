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

## Still to decide

- Exact snapshot fields, encoding, compatibility check, and verification coverage.
- New-player identity, spawn state, admission limits, and concurrent membership requests.
- Snapshot sender selection and agreement among existing peers.
- Chunked transfer over bounded messages, failure handling, and the conditions for resuming.

## Eventual engine abstraction

Prove joining in Erik's game first, then extract reusable mechanisms. Shared joining
and membership operations should let games choose admission and initialization
policy: joining an ongoing world, joining only a lobby, or rejecting joins during a
match. Erik's pause-and-snapshot procedure should not be mandatory for every game.
The concrete engine interfaces remain undecided.
