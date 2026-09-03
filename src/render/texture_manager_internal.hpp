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
    static TextureManager Create(SDL_Renderer* renderer);
    static SDL_Texture* Resolve(const TextureManager& texture_manager, TextureHandle handle);
};

}
