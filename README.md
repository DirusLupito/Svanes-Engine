# Svanes-Engine

Source repository for the Svanes Engine.

## Building from a fresh computer

Install the compiler for your operating system:

- **Windows:** Install [Visual Studio Community 2026](https://visualstudio.microsoft.com/downloads/) with the **Desktop development with C++** workload. Make sure the CMake tools component is included. Visual Studio 2022 is also supported as a fallback.
- **Linux:** Install GCC via your distribution's package manager and [Ninja](https://ninja-build.org/) through your distribution's package manager.
- **macOS:** Install Apple's Clang with `xcode-select --install`, then install [Ninja](https://ninja-build.org/) through a package manager such as Homebrew.

On every platform, also install:

- [CMake](https://cmake.org/download/) 3.25 or newer. The CMake bundled with Visual Studio can be used on Windows.
- The [`just` command runner](https://github.com/casey/just#installation).

After cloning the repository, open a terminal in its root directory and run:

```sh
just fetch-deps
just build
```

On Windows, CMake generates the Visual Studio solution during this process. Open
the solution matching the Visual Studio version selected by the build scripts:

- **Visual Studio 2026:** `out/build/windows-msvc/SvanesEngine.slnx`
- **Visual Studio 2022:** `out/build/windows-vs2022/SvanesEngine.slnx`

Regardless of whether or not you use Visual Studio, you can build and launch optimized Release versions of the games from the command line:

```sh
just run
just run goose
just run orbitalEscalation
just run OrbitalEscalationServer
just run doubleTime-host
just run doubleTime
just run doubleTime-spec
just run doubleTime p2p
```

Use `just debug` instead to build and launch a Debug version:

```sh
just debug
just debug goose
just debug orbitalEscalation
just debug OrbitalEscalationServer
just debug doubleTime
```

Both commands accept the same game targets and default to Orbital Escalation.
To build without launching, use `just release [target]` for Release or
`just build [target]` for Debug. Omit the target to build all games.

## Orbital Escalation multiplayer

Orbital Escalation supports both a dedicated server and P2P. Run the commands
below from the repository root, with each running process in its own terminal.

### Dedicated server

Build and start the server:

```sh
just run OrbitalEscalationServer
```

Build and start a client on the same computer:

```sh
just run OrbitalEscalation --server 127.0.0.1:45010
```

Once both Release executables have been built, skip compilation with:

```sh
just nocompile OrbitalEscalationServer
just nocompile OrbitalEscalation --server 127.0.0.1:45010
```

Repeat the client command in additional terminals to join more players, even
on the same computer. `nocompile` will try to run a previously built Release
executable, and will fail if none exists.

To select another joining port, pass `--port` to the server and use that port
in every client's address:

```sh
just nocompile OrbitalEscalationServer --port 50000
just nocompile OrbitalEscalation --server 127.0.0.1:50000
```

The default joining port is TCP 45010. On another LAN computer, replace `127.0.0.1` 
with the server computer's LAN IPv4 address. 

### P2P

Without `--server`, Orbital Escalation uses P2P lockstep. Start a world, then
join it from another terminal:

```sh
just nocompile OrbitalEscalation
just nocompile OrbitalEscalation --join 127.0.0.1:45000
```

The first process listens on UDP port 45000. The joining process defaults to
45001. Each additional P2P process on the same computer needs a distinct local
port, for example:

```sh
just nocompile OrbitalEscalation --join 127.0.0.1:45000 --port 45002
```

Use an existing peer's LAN address when joining from another computer. These
options also work with `just run OrbitalEscalation` to build before launching.
### Titled Goose Game (Erik's game) multiplayer

Peer-to-peer: one player hosts, others join any player already in the world.
Every copy on one computer needs its own `--port`.

```sh
just run goose
just run goose --join 127.0.0.1:45000 --port 45002
```

Client/server: start the headless server, then connect clients to its IP.

```sh
just run goose-server
just run goose --server 127.0.0.1
```
