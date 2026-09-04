/**
 * Header file for the internal interface of the TextureManager class.
 * This is not intended to be used by external game code.
 * @file texture_manager_internal.hpp
 */

#pragma once

#include <svanes/render/texture_manager.hpp>

struct SDL_Renderer;
struct SDL_Texture;

namespace svanes::internal {

/**
 * Internal interface for the TextureManager class.
 * This class provides methods for creating a TextureManager and resolving texture handles to SDL_Texture pointers.
 * As it deals with raw SDL_Texture pointers, it should not be exposed to game code.
 * The TextureManager class itself is responsible for managing the lifetime of SDL_Texture objects.
 * The TextureManagerInternal class is a friend of TextureManager, allowing it to access private members
 * such as the textures map and the SDL_Renderer pointer.
 */
class TextureManagerInternal final {
public:

    /**
     * Creates a TextureManager instance using the provided SDL_Renderer.
     * Throws an exception if the renderer is null.
     * @param renderer The SDL_Renderer to use for texture management.
     * @return A TextureManager instance.
     * @throws std::invalid_argument if the renderer is null.
     */
    static TextureManager Create(SDL_Renderer* renderer);

    /**
     * Resolves a texture handle to its corresponding SDL_Texture pointer using the provided TextureManager.
     * This should not be exposed to game code, as it returns a raw pointer to an SDL_Texture.
     * Throws an exception if the handle is invalid or does not exist in the manager.
     * @param texture_manager The TextureManager instance to use for resolving the handle.
     * @param handle The TextureHandle to resolve.
     * @return A pointer to the SDL_Texture associated with the handle.
     * @throws std::invalid_argument if the handle is invalid or does not exist.
     */
    static SDL_Texture* Resolve(const TextureManager& texture_manager, TextureHandle handle);
};

}
