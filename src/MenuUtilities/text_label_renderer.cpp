#include "text_label_renderer.hpp"

#include "font_manager_internal.hpp"

#include <svanes/registry.hpp>
#include <svanes/render/render_queue.hpp>
#include <svanes/render/render_system.hpp>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

namespace svanes::internal {

TextLabelRenderer::TextLabelRenderer(TextureManager &textures,
                                     const FontManager &fonts)
    : textures(textures), fonts(fonts) {}

TextLabelRenderer::~TextLabelRenderer() = default;

const TextLabelRenderer::CachedLabel &
TextLabelRenderer::Prepare(Entity entity, const TextLabel &label) {
    const auto found = labels.find(entity);
    if (found != labels.end()) {
        const auto &cached = found->second;

        // Sure we found the label according to the entity, but is it the same
        // label? If the text or font has changed, we need to redo it.
        if (cached.text == label.text && cached.font.id == label.font.id) {
            return cached;
        }
    }

    // We use white text because we can tint it to any color we want when
    // drawing the texture. This also means when changing the color of a text
    // label, we don't need to re-render the texture, as the cached texture is
    // always white and the color is applied at draw time.
    const SDL_Color color{255, 255, 255, 255};

    // unique_ptr for the SDL_Surface returned by TTF_RenderText_Blended_Wrapped
    // using SDL_DestroySurface as the deleter.
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface{
        TTF_RenderText_Blended_Wrapped(
            FontManagerInternal::Resolve(fonts, label.font), label.text.c_str(),
            label.text.size(), color, 0),
        SDL_DestroySurface};

    if (!surface) {
        throw std::runtime_error("Could not render text label: " +
                                 std::string{SDL_GetError()});
    }

    CachedLabel replacementOrNewLabel{
        label.text, label.font,
        OwnedTexture{textures, TextureManagerInternal::CreateFromSurface(
                                   textures, surface.get())},
        surface->w, surface->h};

    return labels.insert_or_assign(entity, std::move(replacementOrNewLabel))
        .first->second;
}

void TextLabelRenderer::Submit(const Registry &world, RenderQueue &queue,
                               Rectangle2D viewport) {

    // If somehow one of our text label entities lost its TextLabel component,
    // we don't want to render it with our cached label. This will clean up
    // any such labels.
    std::erase_if(labels, [&](const auto &entry) {
        return !world.HasComponent<TextLabel>(entry.first);
    });

    // In the future, if labels can be ensured to hold all text labels, and not
    // just a cache of them, we could iterate over the labels map instead of the
    // world. But for now, this is how we catch any newly added text labels or
    // labels with changed text, font, or color.
    world.ForEach<TextLabel>([&](Entity entity, const TextLabel &label) {
        if (!label.visible || label.text.empty()) {
            return;
        }

        if (!std::isfinite(label.position.x) ||
            !std::isfinite(label.position.y)) {
            throw std::invalid_argument("Text label position must be finite.");
        }

        float xAlignment = 0.0F;
        float yAlignment = 0.0F;
        switch (label.alignment) {
        case TextAlignment::TopLeft:
            break;
        case TextAlignment::TopCenter:
            xAlignment = 0.5F;
            break;
        case TextAlignment::TopRight:
            xAlignment = 1.0F;
            break;
        case TextAlignment::CenterLeft:
            yAlignment = 0.5F;
            break;
        case TextAlignment::Center:
            xAlignment = 0.5F;
            yAlignment = 0.5F;
            break;
        case TextAlignment::CenterRight:
            xAlignment = 1.0F;
            yAlignment = 0.5F;
            break;
        case TextAlignment::BottomLeft:
            yAlignment = 1.0F;
            break;
        case TextAlignment::BottomCenter:
            xAlignment = 0.5F;
            yAlignment = 1.0F;
            break;
        case TextAlignment::BottomRight:
            xAlignment = 1.0F;
            yAlignment = 1.0F;
            break;
        default:
            throw std::invalid_argument("Unknown text alignment.");
        }

        const auto &cached = Prepare(entity, label);

        // sdl makes text very nice because it will go ahead and figure
        // out how long a string is in pixels for a given font and font size.
        // massive pain with raw opengl and pure c.
        const float width = static_cast<float>(cached.width);
        const float height = static_cast<float>(cached.height);

        // Begin at the viewport center and subtract half its dimensions to
        // get the viewport's top-left corner. Add the label position to get
        // the label's anchor point, then shift by half the label dimensions so
        // the anchor becomes the rectangle center.
        //
        // Now introduce the alignment values. They change those shifts so the
        // anchor is at the selected corner, edge center, or center of the text.
        // Subtracting xAlignment times the width and yAlignment times the
        // height accounts for how far the anchor is from the text's top-left
        // corner. Rectangle2D still receives the resulting center position.
        const Rectangle2D destination{
            viewport.x - viewport.width * 0.5F + label.position.x +
                (0.5F - xAlignment) * width,
            viewport.y - viewport.height * 0.5F + label.position.y +
                (0.5F - yAlignment) * height,
            width, height};

        const std::int32_t z_order =
            world.HasComponent<ZOrder>(entity)
                ? world.GetComponent<ZOrder>(entity).value
                : 0;

        queue.DrawTexture(cached.texture.GetHandle(), destination, 0.0F,
                          z_order, BlendMode::Alpha, label.color);
    });
}

} // namespace svanes::internal
