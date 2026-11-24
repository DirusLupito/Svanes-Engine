#pragma once

#include <svanes/render/basic_render_types.hpp>

namespace marching_orders {

/**
 * Builds the image the client uses to display the terrain.
 *
 * @return An opaque RGBA image ready for TextureManager::CreateTexture().
 *
 * @throws std::runtime_error if the terrain data is unavailable or invalid.
 * @throws std::length_error if the terrain data is too large to load.
 */
svanes::ImageData LoadTerrainImage();

} // namespace marching_orders
