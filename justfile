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

# Configure and build. Pass a target (sandbox, erik, alex, chris) to build only that game; leave blank to build everything.
build target="": configure
    {{cmake}} --build --preset {{build-preset-prefix}}-debug --parallel {{ if target == "" { "" } else { "--target " + (if target == "sandbox" { "svanes_sandbox" } else if target == "erik" { "svanes_game_erik" } else if target == "alex" { "svanes_game_alex" } else { "svanes_game_chris" }) } }}

# Configure and build all targets in Release mode.
release: configure
    {{cmake}} --build --preset {{build-preset-prefix}}-release --parallel

# Fail fast with a clear message if `run` was given an unknown target.
_check-target target:
    @{{ if target == "" { "" } else if target == "erik" { "" } else if target == "alex" { "" } else if target == "chris" { "" } else { error("no game named '" + target + "'. Try: alex, chris, erik, or leave it blank for the sandbox.") } }}

# Build and launch a game: alex, chris, or erik. Leave blank for the sandbox.
run target="": (_check-target target) (build if target == "" { "sandbox" } else { target })
    {{bin-dir}}{{ if target == "" { "svanes_sandbox" } else if target == "erik" { "svanes_game_erik" } else if target == "alex" { "svanes_game_alex" } else { "svanes_game_chris" } }}{{exe-suffix}}

# Remove compiled outputs while retaining the configured build tree.
clean: configure
    {{cmake}} --build --preset {{build-preset-prefix}}-debug --target clean
