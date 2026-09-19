#include "font_manager_internal.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace svanes {

// Used to instruct unique_ptr to call TTF_CloseFont when the FontPointer goes out of scope.
void FontManager::FontDeleter::operator()(TTF_Font *font) const {
    TTF_CloseFont(font);
}

FontManager::FontManager() {
    if (!TTF_Init()) {
        throw std::runtime_error("Could not initialize SDL_ttf: " +
                                 std::string{SDL_GetError()});
    }
}

FontManager::~FontManager() {
    // Clear the fonts map to ensure all TTF_Font pointers are properly deleted
    // before calling TTF_Quit(). This will invoke the FontDeleter for each
    // loaded font, ensuring that TTF_CloseFont is called for each one.
    fonts.clear();

    // Handles all cleanup of the SDL_ttf library
    TTF_Quit();
}

FontHandle FontManager::LoadFont(std::string_view path, float size_points) {
    if (path.empty() || path.find('\0') != std::string_view::npos) {
        throw std::invalid_argument(
            "Font path must be nonempty and contain no null characters.");
    }

    if (!std::isfinite(size_points) || size_points <= 0.0F) {
        throw std::invalid_argument(
            "Font point size must be finite and positive.");
    }

    if (next_id == 0) {
        throw std::runtime_error("Font handle space is exhausted.");
    }

    // make a unique_ptr to manage the TTF_Font resource, then
    // store it in the fonts map with the next available id, and
    // finally increment the next_id for the next font to be loaded.

    const std::string path_string{path};
    FontPointer font{TTF_OpenFont(path_string.c_str(), size_points)};
    if (font == nullptr) {
        throw std::runtime_error("Could not load font '" + path_string +
                                 "': " + SDL_GetError());
    }

    const FontHandle handle{next_id};
    fonts.emplace(handle.id, std::move(font));
    ++next_id;
    return handle;
}

FontManager internal::FontManagerInternal::Create() { return FontManager{}; }

TTF_Font *
internal::FontManagerInternal::Resolve(const FontManager &font_manager,
                                       FontHandle handle) {
    const auto font = font_manager.fonts.find(handle.id);
    if (font == font_manager.fonts.end()) {
        throw std::invalid_argument("Font handle does not exist.");
    }

    return font->second.get();
}

}
