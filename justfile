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

sandbox-exe := if os() == "windows" {
    "./out/build/windows-msvc/bin/Debug/svanes_sandbox.exe"
} else {
    "./out/build/" + configure-preset + "/bin/Debug/svanes_sandbox"
}

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

# Build and launch the sandbox.
run: build
    {{sandbox-exe}}

# Remove compiled outputs while retaining the configured build tree.
clean: configure
    {{cmake}} --build --preset {{os-prefix}}-debug --target clean
