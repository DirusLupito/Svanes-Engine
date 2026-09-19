#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>

struct TTF_Font;

namespace svanes {

namespace internal {
class FontManagerInternal;
}

/**
 * A handle (reference) to a font loaded by the FontManager.
 *
 * FIELDS:
 * - id: The unique identifier for the font within the FontManager.
 *   This is used to retrieve the font from the FontManager.
 */
struct FontHandle {
    std::uint32_t id = 0;
};

/**
 * Manages the loading and retrieval of fonts for rendering text.
 *
 * This class is responsible for loading fonts from files, storing them,
 * and providing access to the loaded fonts via FontHandle.
 */
class FontManager final {
public:
    /**
     * Destroys the FontManager and releases all loaded fonts.
     * This will free the memory associated with each loaded font,
     * and call TTF_Quit() to clean up the SDL_ttf library.
     */
    ~FontManager();

    /**
     * Override of the copy constructor to prevent copying of FontManager
     * instances.
     *
     * This prevents something like:
     * FontManager fm1;
     * FontManager fm2 = fm1;
     */
    FontManager(const FontManager &) = delete;

    /**
     * Override of the copy assignment operator to prevent copying of
     * FontManager instances.
     *
     * This prevents something like:
     * FontManager fm1;
     * FontManager fm2;
     * fm2 = fm1;
     */
    FontManager &operator=(const FontManager &) = delete;

    /**
     * Override of the move constructor to prevent moving of FontManager
     * instances.
     *
     * This prevents something like:
     * FontManager fm1;
     * FontManager fm2 = std::move(fm1);
     */
    FontManager(FontManager &&) = delete;

    /**
     * Override of the move assignment operator to prevent moving of FontManager
     * instances.
     *
     * This prevents something like:
     * FontManager fm1;
     * FontManager fm2;
     * fm2 = std::move(fm1);
     */
    FontManager &operator=(FontManager &&) = delete;

    /**
     * Loads a font from the specified file path at the given size in points.
     *
     * @param path The file path to the font file. Must be non-empty and contain
     * no null characters.
     * @param size_points The size of the font in points. Must be finite and
     * positive.
     *
     * @return A FontHandle that can be used to reference the loaded font.
     *
     * @throws std::invalid_argument if the path is empty or contains null
     * characters, or if size_points is not finite or not positive.
     * @throws std::runtime_error if the font cannot be loaded or if the font
     * handle space is exhausted.
     */
    FontHandle LoadFont(std::string_view path, float size_points);

private:
    /**
     * A custom deleter for TTF_Font pointers that ensures proper cleanup of
     * font resources.
     *
     * This deleter is used with std::unique_ptr to automatically call
     * TTF_CloseFont when the FontPointer goes out of scope.
     */
    struct FontDeleter {
        void operator()(TTF_Font *font) const;
    };

    // Unique pointer type for managing TTF_Font resources with automatic
    // cleanup.
    using FontPointer = std::unique_ptr<TTF_Font, FontDeleter>;

    /**
     * Private constructor to enforce controlled creation of FontManager
     * instances.
     *
     * This constructor is only accessible by the internal::FontManagerInternal
     * class, which is responsible for creating and managing FontManager
     * instances.
     */
    FontManager();

    // The next unique identifier to assign to a newly loaded font.
    std::uint32_t next_id = 1;

    // The internal storage mapping font identifiers to their corresponding
    // FontPointer. The font manager will use this to store all loaded fonts and
    // provide access to them via FontHandle.
    std::unordered_map<std::uint32_t, FontPointer> fonts;

    // Allows the internal FontManagerInternal class to access private members
    // of FontManager, especially the private constructor and the fonts storage.
    friend class internal::FontManagerInternal;
};

} // namespace svanes
