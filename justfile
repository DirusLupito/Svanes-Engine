set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

cmake := if os() == "windows" {
    "powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File scripts/run-cmake.ps1"
} else {
    "cmake"
}

check-deps-cmd := if os() == "windows" {
    "powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File scripts/check-deps.ps1"
} else {
    "bash scripts/check-deps.sh"
}

configure-preset := if os() == "windows" {
    "windows-msvc"
} else if os() == "macos" {
    "macos-clang"
} else {
    "linux-gcc"
}

os-prefix := if os() == "windows" { "windows" } else if os() == "macos" { "macos" } else { "linux" }

bin-dir := if os() == "windows" {
    "./out/build/windows-msvc/bin/Debug/"
} else {
    "./out/build/" + configure-preset + "/bin/Debug/"
}

exe-suffix := if os() == "windows" { ".exe" } else { "" }

default:
    @just --list

# Verify cmake, a compiler/generator, and vendored third-party sources (currently SDL3) are present.
check-deps:
    {{check-deps-cmd}}

# Generate the build tree (Visual Studio on Windows, Ninja Multi-Config elsewhere).
configure:
    {{cmake}} --preset {{configure-preset}}

# Download/update third-party dependencies (e.g. SDL3) into thirdparty/.
fetch-deps: configure

# Configure and build the debug sandbox.
build: configure
    {{cmake}} --build --preset {{os-prefix}}-debug --parallel

# Configure and build an optimized sandbox.
release: configure
    {{cmake}} --build --preset {{os-prefix}}-release --parallel

# Fail fast with a clear message if `run` was given an unknown target.
_check-target target:
    @{{ if target == "" { "" } else if target == "erik" { "" } else if target == "alex" { "" } else if target == "chris" { "" } else { error("no game named '" + target + "'. Try: alex, chris, erik, or leave it blank for the sandbox.") } }}

# Build and launch a game: alex, chris, or erik. Leave blank for the sandbox.
run target="": (_check-target target) build
    {{bin-dir}}{{ if target == "" { "svanes_sandbox" } else if target == "erik" { "svanes_game_erik" } else if target == "alex" { "svanes_game_alex" } else { "svanes_game_chris" } }}{{exe-suffix}}

# Remove compiled outputs while retaining the configured build tree.
clean: configure
    {{cmake}} --build --preset {{os-prefix}}-debug --target clean
