# Milestone 4 Write-up: Events, Networked Events, and Replay in My Game

Erik Svanes, CSC 581

## How to Play

From `games/goose`, start the server with `just run goose-server`, then start each client with `just run goose <server IPv4>:46000`. Use `127.0.0.1` when the server is on the same machine.

- **A / D** to move, double-tap to dash
- **Space** to jump, or to skip your death replay
- **Left mouse** to fire toward the cursor
- **Esc** to open the menu, which has game speed (only while playing alone), **M** to toggle minions, and **Q** to quit
- **Tab** to switch the camera's scale mode

## 3.1 Engine Design and Rationale

### Event Management

Our engine represents every event as a small C++ class that derives from `svanes::EventOf<T>`, a template over the concrete event type. Each event type declares a single `static constexpr std::string_view EventName`, such as `"goose.death"`. The engine hashes that name at compile time into an `EventTypeId`, and the hash is the only thing the event manager uses to tell types apart. We chose names over a central enum of event types because an enum would have to live in the engine and list every event from each game. With names, a game can define as many event types as it wants without touching engine code, and the same name works as a stable identifier in replay files. The downside is that two event types with the same name look identical to the manager, but names are namespaced by convention (`svanes.replay.play`, `goose.spawn`), which makes a clash unlikely. The replay system rejects a second registration of the same name, but the event manager doesn't, so if two ordinary events had the same name, each one's handlers would also get the other's events and read them as the wrong type. The base `Event` also carries a `priority` and a `Handled` flag, so any handler can stop an event from reaching the handlers after it.

**Registration.** A handler is a `std::function` wrapped in an `EventHandlerWrapper<T>`. That wrapper is handed to `EventManager::Subscribe` as a `std::unique_ptr`, so the manager owns every handler and can't hold a dangling one. Handlers are kept per event type in registration order, so delivery order is predictable. Registering the same callback twice for the same type throws. Subscribing or unsubscribing in the middle of delivery also throws, because that could invalidate the list being walked. Both follow our team's rule of failing loudly when something is used wrong.

**Raising.** There are two ways to raise an event. `TriggerEvent` delivers it immediately to every handler. `QueueEvent` stores it until the next `DispatchEvents`, which delivers queued events in ascending priority order and keeps insertion order between equal priorities. Events queued while a dispatch is running wait for the next dispatch instead of joining the current one. This is the part of the manager I relied on most. A game can make something happen after everything currently queued just by choosing where it calls `DispatchEvents`.

**Timestamps.** Priorities are unsigned 64-bit values, and the assignment asks that they come from our Timeline system. Every event I queue gets the world clock Timeline's current total tics as its priority. The clock can be paused and run at half or double speed, which makes these stamps game time. On the client, events built from network reports carry the server's clock time across the wire, so each one is stamped with the time it happened on the server, which can be a bit before the packet arrives (more on this below).

**Replay is part of the event manager.** The replay system is a `ReplaySession` attached to an `EventManager`. Every delivered event passes through the session's `Observe` first. If the event type was registered with the session (with an encoder and a decoder), the session can record it, and during playback it drops live events of that type so they can't fight with the recorded ones. Replay controls are themselves events (`PlayReplayEvent`, `StopReplayEvent`, and so on), so a game can start or stop a replay from any handler without holding a pointer to the session. The replay system knows nothing about any game's world. All it knows about are events and three callbacks the game provides, which are `save` and `restore` for the starting state and `step` for advancing one simulation step. Any game that draws its world from events can use replay with very little extra work.

**Engine changes for this milestone.** I helped with the design and implementation of the event manager. The event system's overall shape was inspired by Denys Kryvytskyi's event-system tutorial (https://denyskryvytskyi.github.io/event-system), which is credited in the source.

### Network Architecture

The game has been server-authoritative since Milestone 3. Each client sends one `PlayerInput` message per frame, holding what the player wants to do (move, jump, dash, fire, and aim) plus clock and minion settings. The server runs every step (10 ms, so 100 steps per second) and sends each player a full `Snapshot` of the world after each step. I kept this server-centric model for networked events instead of moving to a distributed or hybrid one. The server already decides all the gameplay events that matter, since a death only happens once the server applies the damage. If clients raised those events themselves, I'd have to sort out what happens when two clients disagree, and with the server in charge that never comes up.

The harder question was how to get discrete events across UDP. Our transport has no reliability layer. Snapshots don't need one, because each snapshot is a complete state and a lost one is replaced 10 ms later. A death is different, because if the one packet carrying it is lost, it's gone. I considered adding acknowledgements and retransmission, but that's a lot of work for a handful of events per second. Instead, the server **numbers every death and spawn report and resends it inside every snapshot for 50 steps (half a second)**. Each report is about 30 bytes, so even a busy moment adds very little to a snapshot. The client keeps one counter, the highest report number it has seen. It ignores anything at or below that number and raises everything above it as a local event (`ReportedDeathEvent` or `ReportedSpawnEvent`) on its own event manager. A report is only lost if every snapshot for half a second is lost. I originally planned a 10-step (100 ms) window. I raised it to 50 because in Milestone 3 I played remotely with my brother, from Raleigh to UNC Charlotte, and the connection had choppy stretches. The extra bytes are cheap anyway.

On the server, the relay is just another subscriber. The death and spawn events already have handlers that carry them out, like removing a minion or holding a dead goose. A second, independent handler on each of those event types copies the report into the outgoing window. Neither handler knows about the other, and the networking code ends up subscribed to the same events as the gameplay code.

Input goes the other direction, and I kept it as one full message per frame on purpose. The client still sends its whole `PlayerInput` every frame, and the server turns each one it receives into a `GooseInputEvent` that it queues with a Timeline priority. If the client sent "key pressed" and "key released" events instead, one lost release would leave a goose running forever. With the full input every frame, a lost message gets corrected by the next one. The respawn request rides in the same message as a flag that stays set until it takes effect, for the same reason. Networked events go both ways. Input goes from client to server and is raised as events on the server, and death and spawn reports go from server to client and are raised as events on each client.

### Design Philosophy

The team's main rule is "earn your complexity", which means building the simplest thing that works and only adding more once there's a real need. Most of my design this milestone comes from that. The order of events within a server step is set by where `DispatchEvents` gets called. Deaths and spawns reach clients reliably by being resent inside snapshots we were already sending. The death replay uses the engine's `ReplaySession` as is.

Our other rules show up too. We fail loudly, so if an entity has a collider the contact rules don't recognize, the game throws, and it also throws if something tries to "spawn" the enemy, which exists from the start. We keep data with the system that uses it, so my event types are defined in the game.

Networked reports are ordinary events on the client, so new client features like a kill feed, sound effects, or particles can subscribe to them without touching the networking code. The report already carries the cause of death and the player who got the kill with a kill feed in mind. Replays only need events and the game's callbacks, so the "keep the last N seconds" logic I wrote into my game could move into the engine as a rolling recording mode any game could use.

## 3.2 Individual Game Implementation

### The game

My game is a 2D co-op platformer. Each player controls a goose that runs, jumps, dashes (double-tap a direction), and fires at the mouse cursor. A large enemy patrols the sky firing bullets downward. Every 20–40 seconds it drops a group of three to five minions. Melee minions walk up to the nearest goose and hurt it by touch. Archer minions keep their distance and shoot. The arena has death zones to fall into. A single player can pause the world or run it at half or double speed from the pause menu. With more than one player the world runs at normal speed, so no one can control time for anyone else.

### Showcasing the required features

All four required event types drive gameplay on the server, and each one is handled by whichever system owns the thing it's about.

**Input events.** Every `PlayerInput` that arrives becomes a `GooseInputEvent` stamped with the world clock's time. Its handler joins the client if this is its first input (raising that goose's spawn event), stores the movement intent, latches a dash so it isn't lost between steps, and handles a respawn request from a dead goose by raising a spawn event.

**Collision events.** Each step, the engine's collision detection raises a `CollisionEvent` for every overlapping pair of colliders. I handle these with one rule table. Each side of the collision is given a role (bullet, goose, enemy, minion, death zone, or solid). The two sides are sorted by role, so each rule only has to match one arrangement of its pair. A bullet hitting something is spent and deals damage, except that a bullet never hits its owner, other bullets, or minions its owner spawned. Touching the enemy knocks a goose back without damage, touching a melee minion hurts, and death zones kill. Collisions with the arena's walls and floor are still direct calls. Their result (am I grounded? am I inside a wall?) is needed within the same step, and routing it through events would make grounding depend on handler order. Figures 2 and 3 show the collision rules in a fight.

**Death events.** Damage is applied right away, and what happens because of it is handled with events. A hit lowers health immediately and reports whether *this* hit took the last of it. Only that hit raises a `DeathEvent`, so two lethal hits in the same step only produce one death. Each death carries a report of what died, where, what killed it (enemy fire, player fire, a minion, or a death zone), and, for player fire, which player. Whatever owns the dead thing then handles it. A dead goose stops and waits, the dead enemy loses its collider and disappears from snapshots, and a dead minion is destroyed.

**Spawn events.** Joining, respawning, and minion drops all raise a `SpawnEvent`. The enemy raises one per minion when it drops a group (Figure 2), and the minion system creates the minion when the event is handled.

**Ordering within a step.** The trickiest part of moving to events was making sure no handler sees an entity another handler has already destroyed. Each server step runs in this order:

1. Queue the step's input events and dispatch them.
2. Move the geese, the enemy, and the minions. The enemy may queue minion spawns here.
3. Detect collisions, which queues collision events.
4. Dispatch. Collisions and spawns are handled, and any lethal hit queues a death, which waits because it was raised during a dispatch.
5. Dispatch again. Deaths are handled, so minions are destroyed only after every collision in the step has been handled.
6. Destroy spent bullets and send the snapshot.

Because the event manager never delivers an event raised during the current dispatch, two plain `DispatchEvents` calls in a row give this "collisions, then deaths" order without any priority tricks.

**Networked events.** As described in 3.1, each death and spawn reaches every client as a numbered report, and the client raises it once as its own event. On the client, these events drive what the player sees. Your own reported death turns on the grey death tint and starts your death replay. Your own reported spawn turns it off. In the other direction, the respawn request travels from the client and is acted on inside the server's input event handler.

### The death replay

When your goose dies, your screen replays the last five seconds of your life at normal speed with "REPLAY - SPACE TO SKIP" on screen (Figure 1). When the replay ends, or you press Space, your client asks the server to respawn you.

The replay is built on the engine's `ReplaySession`. I build a `ReplayRecording` by hand and call `ReplaySession::Play` (credit to a teammate, who used this approach first). The client turns every received snapshot into a `SnapshotEvent`, and drawing the world happens in that event's handler. The client also keeps the raw snapshots it has received since your goose last spawned, up to five seconds' worth (500 snapshots at one per server step). When your death report arrives, it makes the oldest kept snapshot the recording's starting state, and every later one becomes one step containing one `SnapshotEvent`. While the replay plays, the engine itself drops the live snapshot events because their type is registered with the session, so the live world can't overwrite the replay. When the replay ends, the next live snapshot just redraws the current world, so there's no state to restore. Skipping raises the engine's `StopReplayEvent`.

The respawn waits for the replay. Originally the server respawned dead geese after a fixed three seconds. That is shorter than the replay, so the goose would have respawned in the middle of its own death replay. I removed the timer and made respawning a request the client sends once the replay is over. I also tried a version where only a gameplay key press (move, jump, or fire) would respawn you, and the replay waited on screen until then. In play it felt worse than respawning automatically, so I went back.

### Why the replay runs on the client

**The replay happens only on the dead player's client**. The server and every other player keep playing normally, since in a co-op game one player dying shouldn't freeze the world for everyone else. Running it on the client also means the server needs no replay state at all, since everything the replay shows is a snapshot the client already received. Figure 4 shows one client replaying its death while the other client keeps playing.

## 3.3 Reflection and Analysis

### Successes and Challenges

The part I'm happiest with is how little special code the networked events and the replay needed once the gameplay ran on events. The relay to clients is two extra subscribers. The death screen is two client handlers, and the replay didn't need any new engine code since it uses the same event the client already draws from. Remote play from Milestone 3 (my server in Raleigh, my brother joining from UNC Charlotte) kept working through all of this, because I only added to the wire format and never changed what was already there.

The most challenging part was **order of effects**, on both sides of the network. On the server, the problem was entities being destroyed while other handlers still needed them, which the two-dispatch step solved. On the client, a replayed snapshot and a live snapshot look identical to the code that draws them. My first version of the death screen worked out whether you were dead by reading your health from each snapshot. During a replay, the replayed snapshots show you alive, so that would have switched the death screen off and asked the server to respawn you in the middle of your own replay. I fixed it by splitting the client in two. Drawing runs for every snapshot, live or replayed, while bookkeeping that should only follow the real present runs only for live snapshots. That covers the death screen fallback, the minion toggle in the menu, and the player count that decides whether the pause menu may change speed.

A related problem was **what happens when a report is lost anyway**. With the half-second window, losing a report takes half a second of total packet loss, but if it happens to your own death, the client never finds out it's dead and never asks to respawn, and the goose stays dead forever. To catch that, the client compares the death screen with your goose's health in the latest live snapshot after the reported events are handled each frame, and corrects it if they disagree. Since the events are dispatched first, this normally does nothing and only takes over when a report actually was lost, in which case you respawn without a replay.

### Critical Evaluation

**Limitations.**

- *The server update step can't raise events.* `ServerUpdateContext` is the only engine context without access to the event manager, so I worked around it in my game by decoding inputs in `Update`, holding them, and queueing them as events in `PhysicsUpdate`.
- *The replay assumes one snapshot per server step.* A dropped snapshot is simply missing from the history, so the replay runs very slightly fast over that gap. Snapshots don't carry a step number. Adding one would let the replay leave gaps at the right length.
- *Full snapshots every step.* Every snapshot is the whole world, and the replay history keeps 500 of them. This is simple and handles loss well, but it wouldn't scale to a large world. Delta snapshots would, at the cost of making both loss handling and replay starting states harder.
- *Resending is time-based, not acknowledged.* The server doesn't know whether a client got a report. Half a second of total loss still drops one. For deaths the snapshot fallback covers this, but anything purely cosmetic (like a future kill feed) would just be missed.

**If I started over**, I would put the rolling "last N seconds" recording into the engine from the start. I hand-build the window and pass it to `ReplaySession::Play`, and Alex's game does something very similar for its own replay. A rolling recording mode on the session (start recording with a step limit, with the starting state rolling forward as old steps drop off) would let both games just raise start, stop and play events. I would also give `ServerUpdateContext` access to the event manager. And I would have the server stamp snapshots with their step number from the beginning, since both replay timing and future interpolation need it. I'd also build the client around events from the start, with snapshots only there to correct them. This time I added events to a client that already read everything from state.

## Appendix: Screenshots

![Figure 1](games/goose/screenshots/figure1_death_replay.png)

**Figure 1.** A death replay in progress, with the grey tint and "REPLAY - SPACE TO SKIP" on screen. The minions on the right are part of the replay too.

![Figure 2](games/goose/screenshots/figure2_minion_drop.png)

**Figure 2.** The enemy has just dropped a group of minions, which are still falling (spawn events). The goose is falling while enemy bullets fly around it (collision events).

![Figure 3](games/goose/screenshots/figure3_health_bars.png)

**Figure 3.** A fight after several drops. The partly red health bars on the minions show damage from bullet hits (collision events), and the bar at the top is the enemy's health.

![Figure 4](games/goose/screenshots/figure4_two_clients.png)

**Figure 4.** Two clients side by side. The left one is replaying its own death while the right one keeps playing live (networked death event, per-client replay).
