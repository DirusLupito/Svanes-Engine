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
just debug doubleTime
```

Both commands accept the same game targets and default to Orbital Escalation.
To build without launching, use `just release [target]` for Release or
`just build [target]` for Debug. Omit the target to build all games.

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
