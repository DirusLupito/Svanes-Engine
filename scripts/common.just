set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

cmake := if os() == "windows" {
    "powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass "
    + "-File scripts/run-cmake.ps1"
} else {
    "cmake"
}

check-deps-cmd := if os() == "windows" {
    "powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass "
    + "-File scripts/check-deps.ps1"
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
    if configure-preset == "windows-msvc" {
        "windows"
    } else {
        "windows-vs2022"
    }
} else if os() == "macos" {
    "macos"
} else {
    "linux"
}

bin-dir := "./out/build/" + configure-preset + "/bin/Debug/"
release-bin-dir := "./out/build/" + configure-preset + "/bin/Release/"

exe-suffix := if os() == "windows" {
    ".exe"
} else {
    ""
}

# Generate the build tree (Visual Studio on Windows, Ninja Multi-Config elsewhere).
configure:
    {{cmake}} --preset {{configure-preset}}


_build targets="": configure
    {{cmake}} --build --preset {{build-preset-prefix}}-debug --parallel {{if targets == "" { "" } else { "--target " + targets }}}

_release targets="": configure
    {{cmake}} --build --preset {{build-preset-prefix}}-release --parallel {{if targets == "" { "" } else { "--target " + targets }}}

_package target: configure
    {{cmake}} --build --preset {{build-preset-prefix}}-release --target package-{{target}} --parallel
