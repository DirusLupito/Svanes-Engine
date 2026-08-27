set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

cmake := if os() == "windows" {
    "powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File scripts/run-cmake.ps1"
} else {
    "cmake"
}

default:
    @just --list

# Generate the Visual Studio/CMake build tree.
configure:
    {{cmake}} --preset windows-msvc

# Download/update third-party dependencies (e.g. SDL3) into thirdparty/.
fetch-deps: configure

# Configure and build the debug sandbox.
build: configure
    {{cmake}} --build --preset debug --parallel

# Configure and build an optimized sandbox.
release: configure
    {{cmake}} --build --preset release --parallel

# Build and launch the sandbox.
run: build
    ./out/build/windows-msvc/bin/Debug/svanes_sandbox.exe

# Remove compiled outputs while retaining the configured build tree.
clean: configure
    {{cmake}} --build --preset debug --target clean
