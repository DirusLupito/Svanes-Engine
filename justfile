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

windows-configure-preset := if os() == "windows" {
    `powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File scripts/run-cmake.ps1 -ResolveWindowsPreset`
} else {
    ""
}

configure-preset := if os() == "windows" {
    windows-configure-preset
} else if os() == "macos" {
    "macos-clang"
} else {
    "linux-gcc"
}

build-preset-prefix := if os() == "windows" {
    if configure-preset == "windows-msvc" { "windows" } else { "windows-vs2022" }
} else if os() == "macos" {
    "macos"
} else {
    "linux"
}

bin-dir := "./out/build/" + configure-preset + "/bin/Debug/"

exe-suffix := if os() == "windows" { ".exe" } else { "" }

default:
    @just --list

# Verify CMake, the compiler/generator, and vendored third-party sources are present.
check-deps:
    {{check-deps-cmd}}

# Generate the build tree (Visual Studio on Windows, Ninja Multi-Config elsewhere).
configure:
    {{cmake}} --preset {{configure-preset}}

# Download/update third-party dependencies into thirdparty/.
fetch-deps: configure

# Configure and build. Pass a target (erik, orbitalEscalation, chris, chris-server) to build only that game; leave blank to build everything.
build target="": (_check-target target) configure
    {{cmake}} --build --preset {{build-preset-prefix}}-debug --parallel {{ if target == "" { "" } else { "--target " + (if target == "erik" { "svanes_game_erik" } else if target == "orbitalEscalation" { "svanes_game_orbital_escalation" } else if target == "chris-server" { "svanes_game_chris_server" } else { "svanes_game_chris" }) } }}

# Configure and build all targets in Release mode.
release: configure
    {{cmake}} --build --preset {{build-preset-prefix}}-release --parallel

# Fail fast with a clear message if an unknown game target was given.
_check-target target:
    @{{ if target == "" { "" } else if target == "erik" { "" } else if target == "orbitalEscalation" { "" } else if target == "chris" { "" } else if target == "chris-server" { "" } else { error("no game named '" + target + "'. Try: chris, chris-server, erik, orbitalEscalation, or leave it blank.") } }}

# Build and launch a game: chris, chris-server, erik, or orbitalEscalation. Leave blank for Orbital Escalation.
# chris launches two clients at once, since it's a two-player game - point chris-server's
# host at each other over a network to test with more than one machine.
run target="": (_check-target target) (build if target == "" { "orbitalEscalation" } else { target })
    {{ if target == "chris" { "just _run-chris-duo" } else { bin-dir + (if target == "" { "svanes_game_orbital_escalation" } else if target == "erik" { "svanes_game_erik" } else if target == "orbitalEscalation" { "svanes_game_orbital_escalation" } else if target == "chris-server" { "svanes_game_chris_server" } else { "svanes_game_chris" }) + exe-suffix } }}

# Launch two chris clients at once. The first is backgrounded so the second still blocks
# the terminal until you close it; the first keeps running until you close its window too.
_run-chris-duo:
    {{ if os() == "windows" { "Start-Process -FilePath " + bin-dir + "svanes_game_chris" + exe-suffix } else { bin-dir + "svanes_game_chris" + exe-suffix + " &" } }}
    {{ if os() == "windows" { "& " + bin-dir + "svanes_game_chris" + exe-suffix } else { bin-dir + "svanes_game_chris" + exe-suffix } }}

# Remove compiled outputs while retaining the configured build tree.
clean: configure
    {{cmake}} --build --preset {{build-preset-prefix}}-debug --target clean
