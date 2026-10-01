#pragma once

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_error.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace svanes {

/**
 * Resolves a packaged asset path from the executable directory.
 * @param relative_path The asset directory or file relative to the executable.
 * @return An absolute path independent of the terminal's working directory.
 * @throws std::runtime_error if SDL cannot locate the executable directory.
 */
inline std::string AssetPath(std::string_view relative_path)
{
    const char* base = SDL_GetBasePath();
    if (!base) {
        throw std::runtime_error(std::string{"Cannot locate game assets: "} + SDL_GetError());
    }
    return (std::filesystem::path{base} / std::filesystem::path{relative_path}).string();
}

}
