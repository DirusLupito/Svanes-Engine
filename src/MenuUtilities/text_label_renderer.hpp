#pragma once

#include "../render/texture_manager_internal.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/entity.hpp>
#include <svanes/rectangle_geometry.hpp>

#include <unordered_map>

namespace svanes {
class Registry;
class RenderQueue;
class TextureManager;
} // namespace svanes

namespace svanes::internal {

/**
 * Responsible for rendering TextLabel components to the screen.
 * This class manages caching of rendered text labels to optimize performance.
 */
class TextLabelRenderer final {
public:
    /**
     * Constructs a TextLabelRenderer with the given TextureManager and
     * FontManager.
     *
     * @param textures The TextureManager used for managing textures.
     * @param fonts The FontManager used for managing fonts.
     */
    TextLabelRenderer(TextureManager &textures, const FontManager &fonts);

    /**
     * Destroys the TextLabelRenderer and releases any cached textures.
     * Destroying the labels map destroys each entry's owning texture handle,
     * which releases its texture through the TextureManager.
     */
    ~TextLabelRenderer();

    /**
     * Override of the copy constructor to prevent copying of TextLabelRenderer
     * instances. This prevents something like:
     *
     * TextLabelRenderer tlr1;
     * TextLabelRenderer tlr2 = tlr1;
     */
    TextLabelRenderer(const TextLabelRenderer &) = delete;

    /**
     * Override of the copy assignment operator to prevent copying of
     * TextLabelRenderer instances. This prevents something like:
     *
     * TextLabelRenderer tlr1;
     * TextLabelRenderer tlr2;
     * tlr2 = tlr1;
     */
    TextLabelRenderer &operator=(const TextLabelRenderer &) = delete;

    /**
     * Submits all visible TextLabel components from the given Registry to the
     * provided RenderQueue for rendering.
     *
     * @param world The Registry containing entities and their components.
     * @param queue The RenderQueue to which the text labels will be submitted.
     * @param viewport The viewport rectangle defining the area in which to
     * render the text labels.
     *
     * @throws std::runtime_error if a text label's position is not finite or if
     * the alignment is invalid.
     */
    void Submit(const Registry &world, RenderQueue &queue,
                Rectangle2D viewport);

private:
    /**
     * Represents a cached text label. Whereas a text label is defined by its
     * text, font, and color, a cached label also includes the texture generated
     * for rendering, as well as its width and height in pixels.
     *
     * FIELDS:
     * - text: The string content of the label.
     * - font: A handle to the font used for rendering the text.
     * - color: The color of the text, including alpha for transparency.
     * - texture: An owning handle to the texture generated for rendering the
     * text. Erasing or replacing the cache entry releases this texture.
     * - width: The width of the rendered text in pixels.
     * - height: The height of the rendered text in pixels.
     */
    struct CachedLabel {
        std::string text;
        FontHandle font;
        Color color;
        OwnedTexture texture;
        std::int32_t width = 0;
        std::int32_t height = 0;
    };

    /**
     * Prepares a CachedLabel for the given entity and TextLabel. If a cached
     * label already exists for the entity, it will be updated if the text,
     * font, or color has changed. If no cached label exists, a new one will be
     * created.
     *
     * @param entity The entity for which to prepare the cached label.
     * @param label The TextLabel component associated with the entity.
     * @return A reference to the prepared CachedLabel.
     *
     * @throws std::runtime_error if the text cannot be rendered to a texture.
     */
    const CachedLabel &Prepare(Entity entity, const TextLabel &label);

    // Reference to the TextureManager used for managing the textures of the
    // cached text labels.
    TextureManager &textures;

    // Reference to the FontManager used for managing the fonts of the cached
    // text labels.
    const FontManager &fonts;

    // A map for every entity that has a cached text label, mapping the entity
    // to its corresponding CachedLabel. Entities here are cached copies of
    // counterparts in the Registry, and should not be treated as authoritative.
    std::unordered_map<Entity, CachedLabel> labels;
};

} // namespace svanes::internal
