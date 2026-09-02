# thirdparty/

Cache directory for third-party dependencies fetched by CMake (`FetchContent`).

Everything under here except this file is gitignored. It's downloaded once
per machine and reused across `just clean` / `just configure` / deleted `out/`
trees, instead of being re-fetched on every build. Run `just fetch-deps` to
populate it without doing a full build.

Currently vendored here:

- **SDL3** - This is the actual SDL3 for windows and rectangles and whatever.
- **SDL3_image** - 
