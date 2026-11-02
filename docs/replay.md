# Event-based replay

`ReplaySession` records driving events and replays fixed simulation steps. It
does not choose keys, render a replay screen, manage a network connection, or
automatically serialize a registry. Existing games are unchanged until they opt in.

## Setup

Include `<svanes/events/replay_session.hpp>` and create a session with the
event manager used for gameplay commands, a positive step duration, and three
callbacks:

```cpp
svanes::ReplaySession replay{
    gameplay_events, simulation_step_tics,
    {
        [&game] { return game.SaveReplayState(); },
        [&game](std::span<const std::byte> state) {
            game.RestoreReplayState(state);
        },
        [&game](svanes::TicCount tics) { game.SimulateStep(tics); },
    }};

replay.RegisterEvent<MoveEvent>(
    [](svanes::MessageWriter &writer, const MoveEvent &event) {
        writer.WriteFloat32(event.direction);
    },
    [](svanes::MessageReader &reader) {
        return std::make_unique<MoveEvent>(reader.ReadFloat32());
    });
```

The encoder writes the event's payload; the decoder reads the same fields and
returns an owning event pointer. The session preserves event type and priority
separately. Decoders must reject invalid field values and consume the entire
payload. Decode must not change the world. Registration is allowed only while
idle, outside event delivery; duplicate event types are errors.

Register only driving events: commands or external decisions needed to run the
simulation again. Leave collisions, damage, and other consequences unregistered
so normal simulation regenerates them. Do not register both a raw input event
and the command its handler produces. Unregistered events continue to work
normally but are not saved. Events with no subscribers are still observed.

`RegisterInputEvents()` registers key, mouse-button, and mouse-motion events
instead of custom commands when raw input is appropriate. These codecs are
opt-in. Input-frame bookkeeping and held input state belong to the game's
simulation and starting-state callbacks.

## Live simulation boundaries

Process control requests every frame, then choose live simulation or playback.
While live, use the following sequence for each completed fixed step:

```cpp
replay.ProcessControls();
if (replay.IsReplaying()) {
    replay.Update(real_delta_tics);
} else if (live_step_due) {
    replay.BeginStep();
    game.DeliverPendingCommands(gameplay_events);
    game.SimulateStep(simulation_step_tics);
    replay.EndStep();
}
```

Deliver registered driving events only between `BeginStep()` and the call to
`SimulateStep()`. Queue and dispatch commands at that input boundary, or trigger
them immediately. `EndStep()` stores the completed step, even if it has no
events. Registered events delivered outside an open recording step are errors.
Do not deliver additional driving events from inside simulation or other
driving-event handlers: replay injects the saved driving sequence before the
simulation callback. Consequence dispatch inside simulation must use the same
schedule during recording and playback.

Control requests take effect in `ProcessControls()`, `BeginStep()`, `EndStep()`,
or `Update()`, outside event callbacks and completed simulation steps. Calling
these operations recursively during delivery or simulation is an error. A
control raised during a replay step takes effect after that step completes.
Recording begins with the next step after the starting-state capture.

The session does not accumulate live time or run live simulation. The game
retains its existing fixed-step pacing; playback `Update()` accumulates elapsed
tics and runs complete recorded steps without discarding overdue steps. Its
elapsed tics are unscaled clock time so paused gameplay cannot prevent resume.

## Controls and lifetime

Trigger or queue these on the associated event manager:

| Event | Effect at the next boundary |
| --- | --- |
| `StartRecordingEvent{path}` | Capture starting state and begin recording |
| `StopRecordingEvent{}` | Save completed steps to the selected file |
| `PlayReplayEvent{path}` | Load and validate events, restore state, begin playback |
| `PauseReplayEvent{}` | Freeze playback and preserve fractional progress |
| `ResumeReplayEvent{}` | Continue paused playback |
| `StopReplayEvent{}` | Stop playback, leaving world state for the game to handle |

Start recording or playback only while idle. Pause requires playing, resume
requires paused, and stopping requires an active recording or playback of the
appropriate kind. Invalid transitions throw. `IsReplaying()` includes paused
playback; `GetState()` also distinguishes idle and recording. Reaching the end
returns to idle without restoring the previous world. Playing the file again
restores its original starting state.

Use one session per event manager, on the simulation thread. Callback resources
must outlive the session. Destroying the session detaches it; destroying the
manager first makes subsequent session operations throw. `Shutdown()` cancels
recording/playback and clears queued events and subscriptions. Stop recording
before shutdown if its data should be saved. A failed save retains the active
recording and propagates the error.

## Input, world state, and assets

Live events of registered driving types are filtered during playback, including
while paused. Unregistered events and replay controls still reach their
subscribers. Use a dedicated gameplay event manager if raw device events must
continue updating UI input independently. Forward live gameplay commands to it
only during the live input boundary. The game controls which world those
handlers address and whether live gameplay runs beside a separate replay world.

Playback start and explicit stop discard the associated manager's pending
events to prevent commands from an old world entering another session. Keep
unrelated UI events on the UI manager. At natural completion, normal simulation
may still have queued consequences; the game decides how to finish or discard
them when leaving replay mode.

Starting state must contain everything needed to repeat simulation: level and
asset references, entity identities, gameplay data, timeline progress, held
controls, and random-generator state where used. Begin recording at a clean
simulation boundary, with no pending gameplay effects that the saved state
cannot reconstruct. Restore must resolve required assets through the game's
existing loading code, validate them before replacing the world, and throw if
they are unavailable. Keep assets in the normal game package; replay files do
not embed textures, audio, or levels. Restoring is responsible for any engine
or game input state used by simulation.

Playback validates all event types and payloads before calling restore or
running simulation. Restore and simulation errors propagate; failed simulation
stops playback. Restore is game code, so the engine cannot roll back partial
changes made by a failing restore callback. File encoding uses explicit fields
through `MessageWriter`/`MessageReader`, with a signature, step duration, initial
state, and a list of steps containing ordered event payloads. Playback requires
the configured step duration to match the recording.

Recordings stay in memory until stop saves them. Playback loads a complete
recording and decodes its events before starting. There are no game-version
checks, cross-platform determinism guarantees, automatic state hashes,
scrubbing, speed controls, compression, or automatic integration with existing
games. Use consistent simulation ordering and repeatable random state; the
engine cannot make arbitrary game logic deterministic.

## Verification

Configure with `-DSVANES_BUILD_REPLAY_TESTS=ON`, build `svanes_replay_tests`, then
run `ctest --test-dir <build-directory> -C <configuration> --output-on-failure`.
The headless tests use synthetic commands, consequences, and starting-state
callbacks. The playable Double Time integration is deferred until its event
migration is complete.
