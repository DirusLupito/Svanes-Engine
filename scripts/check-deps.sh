#!/usr/bin/env bash
set -u

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
min_cmake_version="4.2.0"
ok=1

case "$(uname -s)" in
    Darwin)
        install_compiler="xcode-select --install"
        install_cmake="brew install cmake"
        install_ninja="brew install ninja"
        ;;
    *)
        install_compiler="sudo dnf install gcc-c++"
        install_cmake="sudo dnf install cmake"
        install_ninja="sudo dnf install ninja-build"
        ;;
esac

echo "Checking dependencies..."

version_ge() {
    # true if $1 >= $2
    [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" = "$2" ]
}

if command -v cmake >/dev/null 2>&1; then
    cmake_version="$(cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
    if [ -n "$cmake_version" ] && version_ge "$cmake_version" "$min_cmake_version"; then
        echo "  [ok] cmake found (version $cmake_version)"
    else
        echo "  [missing] cmake $min_cmake_version or newer required (found ${cmake_version:-unknown}) -- $install_cmake"
        ok=0
    fi
else
    echo "  [missing] cmake not found on PATH -- $install_cmake"
    ok=0
fi

if command -v c++ >/dev/null 2>&1 || command -v g++ >/dev/null 2>&1 || command -v clang++ >/dev/null 2>&1; then
    compiler="$(command -v c++ || command -v g++ || command -v clang++)"
    echo "  [ok] C++ compiler found ($compiler)"
else
    echo "  [missing] no C++ compiler found -- $install_compiler"
    ok=0
fi

if command -v ninja >/dev/null 2>&1; then
    echo "  [ok] ninja found ($(command -v ninja))"
else
    echo "  [missing] ninja not found on PATH -- $install_ninja"
    ok=0
fi

if [ -d "$repo_root/thirdparty/sdl3-src" ]; then
    echo "  [ok] SDL3 found (thirdparty/sdl3-src)"
else
    echo "  [missing] SDL3 not fetched yet -- run 'just fetch-deps'"
    ok=0
fi

if [ "$ok" -ne 1 ]; then
    exit 1
fi

echo "All dependencies present."
