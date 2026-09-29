# Goose game: what's left

- **Pause menu.** Only pauses the simulation when playing alone. With other players
  the menu opens but the world keeps running, and the goose gets `GooseIntent{}`
  while the menu is open. A join while paused goes through as normal and ends the
  pause.
- **Join from the menu** instead of the `--join` flag.
- **LAN discovery,** so players don't need to type an address.
- **Port collisions.** ZeroMQ lets two processes share a UDP port without error, so
  copies on one computer need `--port`. Needs a transport that reports a taken port
  or picks a free one.
- **Per-world session ids,** so two worlds on one network can't cross.
- **Maybe join passwords.**

## Engine, when a game needs it

- **Admission rules.** Game-supplied reasons to refuse a join, like "match in
  progress".
- **Attractor order.** `attractor_system.cpp` sums forces in `ForEach` order, which
  differs between players, so a synced game using attractors can desync.
