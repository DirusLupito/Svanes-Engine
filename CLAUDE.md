# Svanes Engine

## Project context

This is a semester project for a Game Engines class. A small team is building
a shared game engine; each member builds their own game on top of it to
demonstrate engine features. Core features, in implementation order:

1. Foundational engine architecture, gameplay frameworks, and loop structures
   (generic entities, input system, etc).
2. Networking models, time management strategies, and multithreading
   capabilities.
3. Runtime object models using object-centric or property-centric approaches.
4. Event management systems, constructed and synchronized.
5. Resource management, input handling, and scripting systems.

## Coding rules

- **No comments in code.** Comments are written by the team, not generated.
- **No `new`/`delete`.** Use smart pointers (`std::unique_ptr`,
  `std::shared_ptr`) for all owned allocations.
- **No platform-specific threading APIs.** Use `std::thread`/`std::jthread`
  (and the rest of `<thread>`/`<stop_token>`), not pthreads/Win32 threads.
- **Nothing platform-specific in general.** The engine must build and run on
  Windows, macOS, and Linux — avoid OS-specific headers/APIs; if a
  platform-specific capability is unavoidable, isolate it behind an
  abstraction rather than spreading `#ifdef`s through engine code.
- **Use `std::stacktrace` when debugging** (e.g. in assertions/error paths),
  not platform-specific backtrace APIs.

## Build system

- CMake + `FetchContent` for dependencies (currently SDL3), `just` as the
  command runner. Presets: `windows-msvc` (Visual Studio), `linux-gcc` /
  `macos-clang` (Ninja Multi-Config) — see `CMakePresets.json`.
- `just check-deps`, `just build`, `just run`, `just release`, `just clean`,
  `just fetch-deps` work identically on all three platforms.
- Third-party sources are cached in `thirdparty/` (gitignored except
  `README.md`) so they aren't re-downloaded on every clean build.
- C++23, no compiler extensions (`CXX_EXTENSIONS OFF`).
