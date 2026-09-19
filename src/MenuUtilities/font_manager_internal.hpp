#pragma once

#include <svanes/MenuUtilities/font_manager.hpp>

namespace svanes::internal {

/**
 * Internal class responsible for creating and managing FontManager instances.
 * This class has access to the private constructor of FontManager, allowing it
 * to create instances of FontManager while preventing external code from doing
 * so.
 */
class FontManagerInternal final {
public:
    /**
     * Creates a new instance of FontManager.
     * This method is the only way to create a FontManager instance, as the
     * constructor of FontManager is private and inaccessible to external code.
     *
     * @return A new instance of FontManager.
     */
    static FontManager Create();

    /**
     * Resolves a FontHandle to the corresponding TTF_Font pointer.
     * This method retrieves the TTF_Font pointer associated with the given
     * FontHandle from the provided FontManager instance.
     *
     * @param font_manager The FontManager instance containing the loaded fonts.
     * @param handle The FontHandle to resolve.
     *
     * @return A pointer to the TTF_Font associated with the given handle.
     *         Returns nullptr if the handle is invalid or does not correspond
     *         to a loaded font.
     */
    static TTF_Font *Resolve(const FontManager &font_manager,
                             FontHandle handle);
};

} // namespace svanes::internal
