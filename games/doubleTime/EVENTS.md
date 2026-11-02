# Double Time events

The production game uses the engine event manager for gameplay commands,
collision response, lifecycle changes, client snapshots, and replays. The
engine replay tests remain in place.

## Server

`DoubleTimeServer` implements `IServer` directly and creates its entities in the
engine-owned registry. It subscribes once during initialization.

- Network input becomes `DoubleTimeCommandEvent`, with the sender identity
  taken from the engine's `ClientInput`, replacing the payload's claimed ID.
- `DoubleTimePlayerSpawnEvent` creates the character when its role is first
  claimed. `DoubleTimePlayerDisconnectedEvent` releases the disconnected
  player's role and removes their character if applicable.
- `CollisionEvent` handlers select and apply the deepest eligible contact for
  each axis, then detection repeats up to four times. The handler normalizes
  contact normals for either entity order. Equal-depth contacts retain the
  stable detection order.
- Terrain contacts come from `DetectTileMapCollisions` and are queued as
  `CollisionEvent`s for the same handler. Platforms and closed doors use
  `DetectEntityCollisions`; terrain does not get a duplicate collider cache.
- A separate probe retains the character's full trigger width while solid
  response uses the narrower physical collider. Hazards, collectibles, and
  goals participate in the engine collision pass.
- Trigger contacts produce collection, respawn, or level-advance events after
  the collision batch completes. Collections are applied before a simultaneous
  hazard respawn. Hazard contacts take precedence over level advancement.

Grounding comes from upward support normals, so ceilings and the jump apex do
not count as ground. Moving-platform carrying follows the supporting platform.
The existing rhythm tolerance, client timing judgments, movement constants,
platform travel/hold behavior, and off-beat penalties are retained.

## Clients and bridge

The server encodes one complete `DoubleTimeWorldSnapshotEvent` payload per
simulation step. Clients decode the latest complete update and dispatch it
through an event handler, following Orbital Escalation's snapshot pattern.
The snapshot includes the authoritative level identity and beat-clock time;
clients do not advance levels independently. A disabled character state removes
the disconnected character from clients.

Local keyboard state comes from the engine's event-backed `InputManager`.
Outgoing commands also pass through `DoubleTimeCommandEvent`. The input bridge
runs through `Application` and handles engine `KeyInputEvent`s. Short actions
are latched until sent/consumed, independently of held controls. Existing input
and bridge packet layouts are retained; the server-to-client snapshot format
has changed, so rebuild both production clients and the server together.

Escape exits replay playback when watching; otherwise it quits. Window
close requests are consumed in game code because the current engine input
interface does not expose them. Packaged assets resolve through `AssetPath`,
independently of the launching working directory.

## Manual verification

Build/run the existing `doubleTime-server`, `doubleTime`, and `doubleTime-input`
targets. Check both roles, short and held inputs, on/off-beat jumps and actions,
floor/wall/ceiling contacts, jump apex and coyote time, platform carrying, door
opening/closing, hazards, collectibles, and level transitions. Disconnect and
rejoin each role, and close the windows normally.

## Replay controls

- `[` starts a server recording. Starting again saves the current recording and
  starts a replacement from the current live state.
- `]` stops and saves the recording.
- `\` saves an active recording and starts playback for the requesting client.
  During playback, `\` or Escape returns to live play. Reaching the end also
  returns automatically.

There is one shared saved recording, `replay.svr`, in the server's SDL
preference directory for organization `Svanes` and application `DoubleTime`.
The server logs that path when saving. All clients can use these controls.
The title bar reports recording and playback status; errors appear in the log.

`DoubleTimeReplayServer` uses engine `ReplaySession` control events and file
encoding. It records authoritative `DoubleTimeWorldSnapshotEvent` deliveries
at the fixed server simulation interval, including the initial snapshot, level
identity, object states, and beat clock. This captures results from both player
roles. Playback reproduces those presentation events; it does not re-simulate
physics from player commands. Assets stay in the normal game package.

Each viewer has an independent server playback session. Live simulation and
network heartbeats continue; viewing does not pause or rewind the shared world.
The viewer's held command is cleared on entry, and gameplay commands are ignored
while viewing. Their character remains in the live simulation and can still
fall, die, or be carried by platforms. The client retains the latest live
snapshot while displaying replay snapshots. On exit it rebuilds the live view
and synchronizes music to the current live beat clock. One-shot actions pressed
while watching are discarded. Disconnecting releases the viewer session.

Verify recording while the other role moves platforms or opens doors, stopping,
playing directly during recording, replacing the saved recording, early exit,
automatic completion, and playback across level changes. While one client
watches, verify the other keeps playing and the viewer returns to that updated
world. Try playback before any recording exists and restart the server to verify
the saved replay persists.
