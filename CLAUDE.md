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
  `std::shared_ptr`, etc) for all owned allocations.
- **Prefer specified width types.** Rather than use int, short, long, etc,
prefer their equivalents with specified widths, int8_t, uint32_t, etc.
- **No platform-specific threading APIs.** Use `std::thread`/`std::jthread`
  (and the rest of `<thread>`/`<stop_token>`), not pthreads/Win32 threads.
- **Nothing platform-specific in general.** The engine must build and run on
  Windows, macOS, and Linux — avoid OS-specific headers/APIs; if a
  platform-specific capability is unavoidable, isolate it behind an
  abstraction rather than spreading `#ifdef`s through engine code.
- **Use `std::stacktrace` when debugging** (e.g. in assertions/error paths),
  not platform-specific backtrace APIs.
- **Data lives with the system that uses it.** Don't split a component/data
  struct into its own header just because it's "data" — that's pointless
  separation. Define it in the header of the system that owns it. A type
  only earns its own header (e.g. under `components/`) once it has enough
  of its own logic to need a `.cpp` file — a bare struct with no behavior
  isn't split out just because it might be shared or used elsewhere
  someday.

## Style guide

- **No trailing underscore on private member variables.** e.g. use
  `count`, not `count_`.
- If statements and else statements should always use curly brackets even if they are not necessary. 
  And for loops and... etc. Blocks should be easily identifiable and not hidden in a single line.
- When writing code, do not try to silently succeed and ignore non-critical errors.
  Crashing with an informative log message is preferable to succeeding in a mysterious way, 
  as such successes can lead to mysterious and difficult to debug issues.


Additionally, a core idea here is that we should earn our complexity.
For example, don't just slap a `[[nodiscard]]` attribute on a function
because "it's good practice elsewhere". 
It's not that we don't care about fancy elements
of the C++ language, but instead that if we don't need to do something
in a complex way, we shouldn't. More than just about anything else,
this should guide everyone and everything developing in this project.
Eventually, maybe we will have a reason to use `[[nodiscard]]`.
But until a clear pattern in which some feature should be used emerges,
we should avoid it.

## Build system

- CMake + `FetchContent` for dependencies (currently SDL3 and SDL3_image),
  `just` as the command runner. Windows prefers `windows-msvc` (Visual
  Studio 2026) and falls back to `windows-vs2022`; `linux-gcc` and
  `macos-clang` use Ninja Multi-Config — see `CMakePresets.json`.
- `just check-deps`, `just build`, `just run`, `just release`, `just clean`,
  `just fetch-deps` work identically on all three platforms.
- Third-party sources are cached in `thirdparty/` (gitignored except
  `README.md`) so they aren't re-downloaded on every clean build.
- C++23, no compiler extensions (`CXX_EXTENSIONS OFF`).
