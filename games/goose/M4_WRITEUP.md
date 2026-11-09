# Milestone 4 Write-up: Events, Networked Events, and Replay in My Game

Erik Svanes, CSC 581

## 3.1 Engine Design and Rationale

### Event Management

Our engine represents every event as a small C++ class that derives from
`svanes::EventOf<T>`, a template over the concrete event type. Each event type
declares a single `static constexpr std::string_view EventName`, such as
`"goose.death"`. The engine hashes that name at compile time into an
`EventTypeId`, and the hash is the only thing the event manager uses to tell
types apart. We chose names over a central enum of event types because an enum
would have to live in the engine and list every game's events. With names, a
game can define as many event types as it likes without touching engine code,
and the same name doubles as a stable identifier for replay files. The cost is
that two event types with the same name would look identical to the manager.
We accepted that because names are namespaced by convention
(`svanes.replay.play`, `goose.spawn`). The replay system does reject a second
registration of the same name, but the event manager itself doesn't check, so
a clash between ordinary events would go unnoticed. That's a gap worth
closing.
The base `Event` also carries a `priority` and a `Handled` flag, so any
handler can stop an event from reaching the handlers after it.

**Registration.** A handler is a `std::function` wrapped in an
`EventHandlerWrapper<T>`. That wrapper is handed to `EventManager::Subscribe`
as a `std::unique_ptr`, so the manager owns every handler and can't hold a
dangling one. Handlers are kept per event type in registration order, so
delivery order is predictable. Registering the same callback twice for the
same type throws. Subscribing or unsubscribing in the middle of delivery also
throws, because that could invalidate the list being walked. Our team's rule
is to fail loudly instead of quietly doing something surprising, and the event
manager follows it throughout.

**Raising.** There are two ways to raise an event. `TriggerEvent` delivers it
immediately to every handler. `QueueEvent` stores it until the next
`DispatchEvents`, which delivers queued events in ascending priority order and
keeps insertion order between equal priorities. Events queued while a
dispatch is running wait for the next dispatch instead of joining the current
one. For me this is the most important property of the manager. It gives
a game a precise way to say "this happens after everything currently queued",
just by choosing where to call `DispatchEvents`.

**Timestamps.** Priorities are unsigned 64-bit values, and the assignment asks
that they come from our Timeline system. Every event I queue gets the
world clock Timeline's current total tics as its priority. Because the clock
can be paused and run at half or double speed, those are game-time stamps, not
wall-clock stamps. On the client, events built from network reports carry the
server's clock time across the wire, so they are stamped with the time the
event happened on the server rather than when the packet arrived (more on this
below).

**Replay is part of the event manager.** The replay system is a
`ReplaySession` attached to an `EventManager`. Every delivered event passes
through the session's `Observe` first. If the event type was registered with
the session (with an encoder and a decoder), the session can record it, and
during playback it drops live events of that type so they can't fight with the
recorded ones. Replay controls are themselves events (`PlayReplayEvent`,
`StopReplayEvent`, and so on), so a game can start or stop a replay from any
handler without holding a pointer to the session. I think this is the design's
best reuse property. The replay system knows nothing about any game's world.
It only knows events and three game-provided callbacks: `save` and `restore`
for the starting state, and `step` for advancing one simulation step. So any
game that draws its world from events gets replay almost for free.

**Engine changes for this milestone.** [Erik: fill in which parts of the event
manager and replay session you personally built or changed, versus your
teammates. The design above is the team's; credit accordingly.] The event
system's overall shape was inspired by Denys Kryvytskyi's event-system
tutorial (https://denyskryvytskyi.github.io/event-system), which is credited
in the source. I made no engine changes this milestone, on
purpose. With the deadline close, every engine change risks breaking a
teammate's game. Our team's rule near a deadline is to build in the game first
and move things into the engine afterwards. Two places where I wanted an
engine change are discussed in 3.3.

### Network Architecture

The game has been server-authoritative since Milestone 3. Each client sends one
`PlayerInput` message per frame, holding what the player wants to do: move,
jump, dash, fire, aim, plus clock and minion settings. The server runs every
step (10 ms, so 100 steps per second) and sends each player a full `Snapshot`
of the world after each step. I kept this server-centric model for networked
events rather than moving to a distributed or hybrid one. All the interesting
gameplay events are decided by the server anyway: a death is only real once
the server has applied the damage. Letting clients raise those events
themselves would mean resolving disagreements between clients, which a
server-centric model avoids entirely.

The harder question was how to get discrete events across UDP. Our transport
has no reliability layer. Snapshots don't need one, because each snapshot is a
complete state and a lost one is replaced 10 ms later. A death is different:
if the one packet carrying it is lost, it's gone. I considered adding
acknowledgements and retransmission, but that's a lot of machinery for a
handful of events per second. Instead, the server **numbers every death and
spawn report and resends it inside every snapshot for 50 steps (half a
second)**. Each report is about 30 bytes, so even a busy moment adds very
little to a snapshot. The client keeps one counter, the highest report number
it has seen. It ignores anything at or below that number and raises everything
above it as a local event (`ReportedDeathEvent` or `ReportedSpawnEvent`) on
its own event manager. A report is only lost if every snapshot for half a
second is lost. I originally planned a 10-step (100 ms) window. I raised it to
50 because in Milestone 3 I played remotely with my brother, from Raleigh
all the way to UNC Charlotte, and the link had choppy stretches, and the extra bytes are cheap.

On the server, the relay is just another subscriber. The death and spawn
events already have handlers that carry them out: removing a minion, holding
a dead goose, and so on. A second, independent handler on each of those event
types copies the report into the outgoing window. Neither handler knows the
other exists. This is the main way the network and the event manager work
together. Networking is not a special path; it is one more listener
on the same events the gameplay uses.

Input goes the other direction, and I deliberately did **not** turn it into
discrete networked events. The client still sends its whole `PlayerInput`
every frame, and the server turns each received input into a `GooseInputEvent`
that it queues with a Timeline priority. If the client instead sent "key
pressed" and "key released" events, one lost release would leave a goose
running forever. Sending the full intent each frame means a lost message is
corrected by the next one. The respawn request rides in the same message as a
flag that stays set until it takes effect, for the same reason. So the
networked events go both ways: client→server as input events raised on the
server, and server→client as death and spawn reports raised on each client.

### Design Philosophy

The team's guiding rule is "earn your complexity": build the simplest thing
that works, and only add machinery once a real need shows up. Most of my
M4 design follows from that rule. Event ordering within a server step comes
from *where* `DispatchEvents` is called, not from priority arithmetic.
Reliable events come from resending inside messages we already send, not from
a new reliability layer. And the death replay reuses the engine's
`ReplaySession` instead of a game-side playback system.

Our other rules show up too. We fail loudly: an entity with a collider that
the contact rules don't recognize throws instead of being ignored, and so
does an attempt to "spawn" the enemy, which exists from the start. We keep
data with the system that uses it, so my event types live in the
game, not the engine. And we prefer not to change the engine near deadlines.

These choices leave room for future work. Because networked reports are
ordinary events on the client, new client features such as a kill feed, sound
effects or particles can subscribe to them without touching the networking
code. The report already carries the cause of death and the killing player for
exactly that reason. And because replays only need events plus a restore
callback, the "keep the last N seconds" logic I build by hand could
move into the engine as a rolling recording mode that any game could use.

## 3.2 Individual Game Implementation

### The game

My game is a 2D co-op platformer. Each player controls a goose that runs,
jumps, dashes (double-tap a direction), and fires at the mouse cursor. A large
enemy patrols the sky firing bullets downward. Every 20–40 seconds it drops a
group of three to five minions. Melee minions walk up to the nearest goose and
hurt it by touch. Archer minions keep their distance and shoot. The arena has
death zones to fall into. A single player can pause the world or run it at
half or double speed from the pause menu. With more than one player the world
runs at normal speed, so no one can control time for anyone else.

### Showcasing the required features

All four required event types drive real gameplay on the server, and each is
handled by whatever owns the thing it is about.

**Input events.** Every `PlayerInput` that arrives becomes a `GooseInputEvent`
stamped with the world clock's time. Its handler joins the client if this is
its first input (raising that goose's spawn event), stores the movement
intent, latches a dash so it isn't lost between steps, and handles a respawn
request from a dead goose by raising a spawn event.

**Collision events.** Each step, the engine's collision detection raises a
`CollisionEvent` for every overlapping pair of colliders. I handle these
with one rule table. Each side of the collision is given a role (bullet,
goose, enemy, minion, death zone, or solid). The two sides are sorted by role,
so each rule only has to match one arrangement of its pair. A bullet hitting
something is spent and deals damage, with a few exceptions: a bullet never
hits its owner, other bullets, or minions its owner spawned. Touching the
enemy knocks a goose back without damage, touching a melee minion hurts, and
death zones kill. Collisions with the arena's walls and floor stay as direct
calls instead of events. Their result (am I grounded? am I inside a wall?) is
needed within the same step, and routing it through events would make
grounding depend on handler order. Figure 3 shows the collision rules in a
fight.

**Death events.** Damage is applied immediately, but its consequences are
events. A hit lowers health right away and reports whether *this* hit took
the last of it. Only that hit raises a `DeathEvent`, so two lethal hits in the
same step produce one death, not two. Each death carries a report of what
died, where, what killed it (enemy fire, player fire, a minion, or a death
zone), and, for player fire, which player. The owner then carries it out: a
dead goose stops and waits, the dead enemy loses its collider and disappears
from snapshots, and a dead minion is destroyed.

**Spawn events.** Joining, respawning, and minion drops all raise a
`SpawnEvent`. The enemy raises one per minion when it drops a group (Figure
2), and the minion system creates the minion when the event is handled.

**Ordering within a step.** The trickiest part of moving to events was
making sure no handler sees an entity another handler has already destroyed.
Each server step runs in a fixed order:

1. Queue the step's input events and dispatch them.
2. Move the geese, the enemy, and the minions. The enemy may queue minion
   spawns here.
3. Detect collisions, which queues collision events.
4. Dispatch. Collisions and spawns are handled, and any lethal hit queues a
   death, which waits because it was raised during a dispatch.
5. Dispatch again. Deaths are handled, so minions are destroyed only after
   every collision in the step has been handled.
6. Destroy spent bullets and send the snapshot.

Because the event manager never delivers an event raised during the current
dispatch, two plain `DispatchEvents` calls in a row give exactly this
"collisions, then deaths" order, with no priority tricks.

**Networked events.** As described in 3.1, each death and spawn reaches every
client as a numbered report, and the client raises it once as its own event.
On the client, these events drive what the player sees. Your own reported
death turns on the grey death tint and starts your death replay. Your own
reported spawn turns it off. In the other direction, the respawn request
travels from the client and is acted on inside the server's input event
handler.

### The death replay

When your goose dies, your screen replays the last five seconds of your life
at normal speed with "REPLAY - SPACE TO SKIP" on screen (Figure 1). When the
replay ends, or you press Space, your client asks the server to respawn you.

The replay is built on the engine's `ReplaySession`: I build a
`ReplayRecording` by hand and call `ReplaySession::Play` (credit to a
teammate, who used this approach first). The client turns
every received snapshot into a `SnapshotEvent`, and drawing the world happens
in that event's handler. The client also keeps the raw snapshots it has
received since your goose last spawned, up to five seconds' worth (500
snapshots at one per server step). When your death report arrives, it makes
the oldest kept snapshot the recording's starting state, and every later one
becomes one step containing one `SnapshotEvent`. While the replay plays, the
engine itself drops the live snapshot events because their type is
registered with the session, so the live world can't overwrite the replay.
When the replay ends, the next live snapshot simply redraws the present, so
there is no state to restore. Skipping raises the engine's `StopReplayEvent`.

The respawn waits for the replay. Originally the server respawned dead geese
after a fixed three seconds. That is shorter than the replay, so the goose
would have respawned in the middle of its own death replay. I removed the
timer and made respawning a request the client sends once the replay is over.
I also tried a version where only a gameplay key press (move, jump, or fire)
would respawn you, and the replay waited on screen until then. In play it
felt worse than respawning automatically, so I went back.

### Why the replay runs on the client

**The replay happens only on the dead player's client**. The server
and every other player keep playing normally, which is what a co-op game
needs: one player's death shouldn't freeze the world for everyone. Running it
on the client also means the server needs no replay state at all, since
everything the replay shows is a snapshot the client already received.
Figure 4 shows one client replaying its death while the other client keeps
playing.

## 3.3 Reflection and Analysis

### Successes and Challenges

The part I'm happiest with is how little special code the networked events
and the replay needed once the gameplay ran on events. The relay to clients is
two extra subscribers. The death screen is two client handlers. And the replay
didn't need any new engine code: it uses the same event the client already
draws from. Remote play from Milestone 3 (my server in Raleigh, my brother
joining from UNC Charlotte) kept working through all of this, because the wire
format only grew instead of changing shape.

The most challenging part was **order of effects**, on both sides of the
network. On the server, the challenge was entities being destroyed while
other handlers still needed them, which the two-dispatch step solved. On the
client, a replayed snapshot and a live snapshot look identical to the code
that draws them. My first version of the death screen worked out whether you
were dead by reading your health from each snapshot. During a replay, the
replayed snapshots show you alive, so that would have switched the death
screen off and asked the server to respawn you in the middle of your own
replay. The fix was to split the client in two. Drawing runs for every
snapshot, live or replayed. Bookkeeping that should only follow the real
present runs only for live snapshots: the death screen fallback, the minion
toggle in the menu, and the player count that decides whether the pause menu
may change speed.

A related challenge was **what happens when a report is lost anyway**. With
the half-second window, losing a report takes half a second of total packet
loss. When that happens to your own death, though, the client never learns
it's dead and never asks to respawn, so the goose would stay dead forever. I
added a fallback. After the reported events are handled each frame, the
client compares the death screen with your goose's health in the latest live
snapshot and corrects it if they disagree. The events are dispatched first,
so normally they are in charge and the fallback does nothing. It only takes
over when a report really was lost, in which case you respawn without a
replay. The design is "events first, state as a safety net", and I think that
is the right shape for networked events over UDP in general.

Finally, at the start of the milestone the game didn't even compile. The engine's
input headers had moved while I wasn't touching the game. That was a reminder
that a shared engine changes under every game built on it, and it's part of
why I avoided engine changes this time.

### Critical Evaluation

**Limitations.**

- *The server update step can't raise events.* `ServerUpdateContext` is the
  only engine context without access to the event manager. I have to
  decode inputs in `Update`, hold them, and queue them as events in
  `PhysicsUpdate`. It works, but it's a workaround for a one-line gap in our
  engine. I chose not to fix it this close to submission.
- *The replay assumes one snapshot per server step.* A dropped snapshot is
  simply missing from the history, so the replay runs very slightly fast over
  that gap. Snapshots don't carry a step number. Adding one would let the
  replay leave gaps at the right length.
- *Full snapshots every step.* Every snapshot is the whole world, and the
  replay history keeps 500 of them. This is simple and robust to loss, but it
  wouldn't scale to a large world. Delta snapshots would, at the cost of
  making both loss handling and replay starting states harder.
- *Resending is time-based, not acknowledged.* The server doesn't know
  whether a client got a report. Half a second of total loss still drops one.
  For deaths the snapshot fallback covers this; for anything purely cosmetic
  (a future kill feed), it would just be missed.
- *Not finished.* I planned a kill feed driven by the reported death events
  (the reports already carry the cause and the killer) and a dash driven by
  the engine's key events on the client. Neither made the deadline.

**If I started over**, I would put the rolling "last N seconds" recording into
the engine from the start. I hand-build the window and pass it to
`ReplaySession::Play`, and Alex's game takes a very similar approach to its
own replay. Two games building the same thing by hand is a good sign it
belongs in the engine. A rolling recording mode on the session (start
recording with a step limit, with the starting state rolling forward as old
steps drop off) would let both games simply raise start, stop and play
events. I designed exactly that early in the milestone
and dropped it only because of the deadline. I would also give
`ServerUpdateContext` access to the event manager. And I would have the
server stamp snapshots with their step number from the beginning, since both
replay timing and future interpolation want it. More broadly, I'd design the
client around "events change what you see, snapshots correct it" from day
one, instead of retrofitting events onto a client that read everything from
state.

## Appendix: Screenshots

**Figure 1.** [SCREENSHOT PLACEHOLDER: a death replay in progress, with the
grey tint and "REPLAY - SPACE TO SKIP" visible, ideally with bullets or
minions on screen.]

**Figure 2.** [SCREENSHOT PLACEHOLDER: the enemy dropping a group of minions
(spawn events).]

**Figure 3.** [SCREENSHOT PLACEHOLDER: combat. A goose shooting minions, with
damaged minions showing health bars and an enemy bullet near the goose
(collision events).]

**Figure 4.** [SCREENSHOT PLACEHOLDER: two clients side by side, with one
replaying its death while the other keeps playing (networked death event,
per-client replay).]

**Figure 5 (optional).** [SCREENSHOT PLACEHOLDER: the server console showing
players joining, or the pause menu.]
