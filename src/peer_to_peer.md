## Peer-to-Peer Multiplayer

Every player runs the whole game. Only inputs cross the network: each tick, every player sends their input to everyone else, and every copy of the game steps with the same inputs. If every copy starts from the same world and steps the same way, every copy stays the same.

There is no host in charge. Anyone in the world can let a new player in.

### Setup

```cpp
svanes::SyncSettings sync_settings{.step_tics = 10000};
svanes::PeerSettings settings{session_id, max_players, rules_hash};

auto pipe = std::make_unique<svanes::UdpMsgPipe>(port);
peers = joining ? std::make_unique<svanes::PeerGroup>(std::move(pipe), address, settings)
                : std::make_unique<svanes::PeerGroup>(std::move(pipe), settings);
sync = std::make_unique<svanes::InputSync<MyInput, MyInputUse>>(*peers, *simulation, sync_settings);

// every frame
sync->Update(frame);
```

- [PeerGroup](../include/svanes/network/peer_group.hpp) handles who is in the world: joining, leaving, and players who disconnect.
- [InputSync](../include/svanes/network/input_sync.hpp) exchanges inputs and steps your simulation.
- `rules_hash` is a hash of anything two builds must agree on (step size, gravity, `SyncSettings`). Players with a different hash can't join.
- `sync->RequestLeave()` leaves; close the game once `sync->CanClose()` is true.

### What your game implements

Your simulation derives from `svanes::SyncedSimulation<Input, Use>`. `Input` is one player's controls for one tick. `Use` says which parts of an input a tick actually read (an empty struct is fine).

| Method | What it does |
|---|---|
| `Save` / `Load` | The whole world as bytes, and back. |
| `Step` | Advance one tick with every player's input. |
| `CaptureInput` | Read the keyboard/mouse. Called once per frame. |
| `TakeInput` | This player's input for the next tick. Called once per tick. |
| `EncodeInput` / `DecodeInput` | Input to bytes, and back. |
| `AddPlayer` / `RemovePlayer` | A player joined or left. |
| `Predict` (optional) | Guess a missing input. Defaults to repeating the last one. |
| `ChangesStep` (optional) | Could this input difference have changed the tick? Defaults to "any difference". |

`Input{}` must mean "nothing pressed".

Missing inputs are guessed so the game doesn't wait. When the real input arrives and the guess was wrong, the world is rewound and replayed. Setting `prediction_ticks = 0` turns this off: everyone waits for every input instead (lockstep).

### Keeping the simulation deterministic

Every copy must produce exactly the same world. Every 100 ticks the copies compare world hashes, and the game stops with "State mismatch" if they differ.

- Use `svanes::Sin`, `svanes::Cos`, and `svanes::Length` from [deterministic_math](../include/svanes/deterministic_math.hpp), not `std::sin`/`std::cos`/`std::hypot`, which differ between platforms.
- Don't depend on `Registry::ForEach` order or entity ids; they differ between players. Give entities a `StableId` and use `EntitiesByStableId` ([stable_id](../include/svanes/stable_id.hpp)).
- Don't read real time, random numbers, or anything local (window size, camera, mouse) inside `Step`. Only the world and the inputs.
- `Save` must include everything `Step` reads. Anything left out will differ on a player who joins late or after a rewind. [component_serialization](../include/svanes/network/component_serialization.hpp) writes the common engine components.
