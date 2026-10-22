# HW2 Decisions

The Titled Goose Game (my game) now runs two multiplayer models over one simulation. In peer-to-peer, every player runs the world and only inputs cross the network. In client/server, a headless server runs the world and sends it to everyone.

The commands to run it are below.

```sh
###################
#       P2P       #
###################
just run goose                                      # start a peer-to-peer world
just run goose --join 127.0.0.1:45000 --port 45002  # join it (each copy on one computer needs its own port)

###################
# Client / Server #
###################
just run goose-server                               # start the headless server
just run goose --server 127.0.0.1                   # connect a client to it
```

## Section 1 (Time)

One of my teammates wrote the engine's Timeline, which can pause and change its tic size, so my job in the game was deciding what its clock should drive. I gave the goose game one world clock, a Timeline that I advance by real time each frame. I hand the simulation that clock's delta instead of the frame's raw delta, so every moving object (geese, the enemy, bullets) moves by game time, and I didn't have to change anything else to pause or scale it.

I added a pause menu that opens with Escape. Opening it pauses the game, and in it you pick the speed (0.5x, 1x, 2x) with the arrow keys and Enter or with the mouse, and I show the current speed in a corner label.

I decided pause and speed only apply when you play alone. A shared world has one pace, so if one player could slow or freeze it, they would slow or freeze everyone else. With other players present the menu still opens, your goose stops taking input, and the speed row is greyed out. If a player joins while you are at 2x, I snap you back to 1x, and if everyone else leaves while your menu is open, I pause the game.

The snap is a compromise. The rule I wanted was to refuse joiners unless the host is at 1x, but the engine has no hook for a game to refuse a join, and I chose not to change shared engine code right before submission. I built all of this inside the game, and a join-refusal hook is something I'm looking to add.

## Section 5 (Peer-to-Peer, no central authority)

I built a fair amount of this part of the engine myself (PeerGroup and InputSync), and we decided on the extra-credit option. There is no server and no host with authority.

I only send inputs across the network. Every player runs the whole simulation, and each tick every player sends their input to everyone else. If every copy starts from the same world and steps with the same inputs, every copy stays the same, so there is never a question of whose world is correct. The enemy, my equivalent of the moving platform, needs no owner. I compute its path from the tick number, so every copy moves it identically without anyone sending its position. In future milestones I plan on actually giving the enemy attack patterns, which will need to be handled differently.

I let anyone in the world admit a new player. A joiner contacts any member, which becomes its sponsor, assigns it an id, tells it everyone else's address, and sends it the world. Changing who is in the world was the hardest part to get right without an authority, since everyone has to apply the change at the same tick. I made PeerGroup freeze every member, agree on a boundary tick (the latest tick anyone froze at), wait until everyone has settled exactly there, compare hashes of the saved world, and only then apply the change everywhere together. A member that stops responding gets dropped through the same process instead of breaking the world. A build with different rules (step size, gravity, sync settings) has a different rules hash, and I don't let it join.

Waiting on every input each tick would make the game feel as slow as its slowest connection, so I made InputSync guess missing inputs (by repeating the last one) and keep going. When the real input arrives and the guess was wrong, it rewinds the world to that tick and replays it.

All of this depends on determinism. Every copy has to produce the exact same world from the same inputs, and that shaped the whole game. I added deterministic sin, cos and length to the engine to use instead of the standard library's, which differ between platforms. Entity ids and iteration order differ between players, so I made sure nothing in a step depends on them, and I added StableId for the entities that matter. Every 100 ticks the players compare world hashes, and I stop the game with a state mismatch rather than let each peer drift apart silently.

## Sections 2 and 4 (Client/Server and Asynchronicity)

My teammates built most of the engine's server side, so I've only looked into it at a higher level. The engine gives me a NetworkServer with one thread per client connection and a server loop that steps a simulation at a fixed rate.

I made the goose server run the same GooseSimulation as peer-to-peer, but alone, with authority, and with no window. Clients send their input. Every step the server advances the world using each client's latest input and sends every client a snapshot of the world, which the client loads and draws. Because my simulation already had Save, Load, AddPlayer and RemovePlayer for peer-to-peer, I needed almost no new game logic for the server.

I gave the server 8 slots, each with its own port and thread. Clients can start at any time. A client tries each slot in turn until one accepts it, and its goose only exists while it holds the slot. Leaving, or 3 seconds of silence, frees the slot and removes the goose. Every snapshot carries the list of players, so the other clients add and remove geese to match.

For Section 4, I let each client change its own loop rate (15, 60 or 120 frames per second) from the menu, using engine support for variable loop timing that one of my teammates added. A client sends one input message per frame, so a 15 fps client sends a quarter of the messages of a 60 fps one. The server never waits on any client. Each client has its own thread, and the server steps on its own clock with whatever input each client last sent, so a slow client only makes its own view choppier. I kept the same solo rule here too, so while one client is alone on the server, its pause and speed control the server's world clock.

## Section 3 (Threads)

I run two threads with distinct jobs on the goose server. The main thread steps the simulation. A second thread, which I added in the game, encodes each finished world into a snapshot and sends it to every client. They share only one slot holding the newest world, guarded by a mutex and a condition variable, and a newer world replaces one not yet sent, since clients only need the latest.

I deliberately did not split the simulation itself across threads, such as geese on one thread and the enemy on another. The engine's registry is not thread-safe, and geese and the enemy both create bullets and change components mid-step. Making that safe means separating each step into a parallel decide phase and an ordered apply phase, and if I got the order wrong, peer-to-peer would desync. I think it could be a next step, but it was getting too close to the deadline, so I put the second thread where the data is naturally separate.

## What worked and what didn't

Building both models on one simulation interface worked better than I expected, and keeping the menu and clock rules identical across both made them easy for me to reason about.

My rule of not touching engine code before submission cost some cleanliness. Since I spent so much time on peer-to-peer and had less time for other features, the clock feeding InputSync and the snap to 1x on join are workarounds I built in the game that belong in the engine.

My client/server clients only show snapshots, without predicting their own goose, so there is a little input lag. I could reuse my peer-to-peer rollback machinery for client-side prediction.

My server has a fixed 8 slots. One of my teammates has since added a shared joining port to the engine that removes the cap, and I can move goose to it once the engine can also close a departed client's connection.

One bug is still unsolved. In client/server at 15 fps, an input very occasionally does not register. I suspected short taps falling between frames, but counting presses within a frame did not fix it, and I ran out of time to track it down.
