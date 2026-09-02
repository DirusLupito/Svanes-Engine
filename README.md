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

Then launch the sandbox or one of the games:

```sh
just run
just run erik
just run alex
just run chris
```
