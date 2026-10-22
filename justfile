# List available commands.
default:
    @just --list

import 'scripts/common.just'

# Verify CMake, the compiler/generator, and vendored third-party sources are present.
check-deps:
    {{check-deps-cmd}}

# Download/update third-party dependencies into thirdparty/.
fetch-deps: configure

# Remove compiled outputs while retaining the configured build tree.
clean: configure
    {{cmake}} --build --preset {{build-preset-prefix}}-debug --target clean
