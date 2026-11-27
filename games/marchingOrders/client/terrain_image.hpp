#pragma once

#include "../terrain.hpp"

#include <svanes/render/basic_render_types.hpp>

namespace marching_orders {

/**
 * Builds the image the client uses to display the terrain.
 *
 * @param terrain The elevation grid for the match.
 *
 * @return An opaque RGBA image ready for TextureManager::CreateTexture().
 *
 * @throws std::runtime_error if the terrain data is unavailable or invalid.
 * @throws std::length_error if the terrain data is too large to load.
 */
svanes::ImageData LoadTerrainImage(const Terrain &terrain = Terrain::Get());

} // namespace marching_orders
