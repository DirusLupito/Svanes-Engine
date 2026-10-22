# Svanes-Engine

Source repository for the Svanes Engine.

## Building from a fresh computer

Install the compiler for your operating system:

- **Windows:** Install [Visual Studio Community 2026](https://visualstudio.microsoft.com/downloads/) with the **Desktop development with C++** workload. Make sure the CMake tools component is included. Visual Studio 2022 is also supported as a fallback.
- **Linux:** Install GCC via your distribution's package manager and [Ninja](https://ninja-build.org/) through your distribution's package manager.
- **macOS:** Install Apple's Clang with `xcode-select --install`, then install [Ninja](https://ninja-build.org/) through a package manager such as Homebrew.

On every platform, also install:

- [CMake](https://cmake.org/download/) 3.26 or newer. The CMake bundled with Visual Studio can be used on Windows.
- The [`just` command runner](https://github.com/casey/just#installation).

After cloning the repository, open a terminal in its root directory and run:

```sh
just fetch-deps
```

Then build your desired game:
```sh
just --justfile games/doubleTime/justfile build
just --justfile games/goose/justfile build
just --justfile games/orbitalEscalation/justfile build
```

On Windows, CMake generates the Visual Studio solution during this process. Open
the solution matching the Visual Studio version selected by the build scripts:

- **Visual Studio 2026:** `out/build/windows-msvc/SvanesEngine.slnx`
- **Visual Studio 2022:** `out/build/windows-vs2022/SvanesEngine.sln`

Regardless of whether or not you use Visual Studio, you can build and launch
optimized versions with debug symbols (`RelWithDebInfo`) from the command line:

```sh
just --justfile games/orbitalEscalation/justfile run OrbitalEscalationServer
just --justfile games/orbitalEscalation/justfile run OrbitalEscalation --server 127.0.0.1:45010
just --justfile games/goose/justfile run
just --justfile games/doubleTime/justfile run doubleTime-server
just --justfile games/doubleTime/justfile run doubleTime-input
just --justfile games/doubleTime/justfile run doubleTime
```

Use `just debug` instead to build and launch a Debug version:

```sh
just --justfile games/orbitalEscalation/justfile debug OrbitalEscalationServer
just --justfile games/goose/justfile debug
just --justfile games/doubleTime/justfile debug
```

You can also run `just` from a game's directory if you don't want to specify `--justfile`.
Running `just build`, `just release`, `just run`, or `just debug` from a game
directory defaults to that game. Orbital Escalation clients require a server
address; see below. For example, from `games/doubleTime`:

```sh
just build
just run
just run doubleTime
```

## Orbital Escalation multiplayer

Orbital Escalation uses a dedicated server.
Run the commands below from `games/orbitalEscalation`, with each running
process in its own terminal.

Build and start the server:

```sh
just run OrbitalEscalationServer
```

Build and start a client on the same computer:

```sh
just run OrbitalEscalation --server 127.0.0.1:45010
```

Once both `RelWithDebInfo` executables have been built, skip compilation with:

```sh
just nocompile OrbitalEscalationServer
just nocompile OrbitalEscalation --server 127.0.0.1:45010
```

Repeat the client command in additional terminals to join more players, even
on the same computer. `nocompile` runs a previously built `RelWithDebInfo`
executable, and will fail if none exists. The client requires `--server` and
chooses its local UDP port automatically.

To select another server port, pass `--port` to the server and use that port
in every client's address:

```sh
just nocompile OrbitalEscalationServer --port 50000
just nocompile OrbitalEscalation --server 127.0.0.1:50000
```

The default server port is UDP 45010, used for both admission and gameplay.
On another LAN computer, replace `127.0.0.1` with the server computer's LAN IPv4
address. For players outside the LAN, forward that UDP port to the server and
give them its public IPv4 address. Allow the server through its firewall.


Use an existing peer's LAN address when joining from another computer. These
options also work with `just run OrbitalEscalation` to build before launching.
### Titled Goose Game (Erik's game) multiplayer

Run the commands below from `games/goose`. Start the headless server, which
listens on UDP port 46000, then connect any number of clients to it.

```sh
just run goose-server
just run goose 127.0.0.1:46000
```
