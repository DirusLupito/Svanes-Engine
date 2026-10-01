/**
 * Header file for the internal interface of the TextureManager class.
 * This is not intended to be used by external game code.
 * @file texture_manager_internal.hpp
 */

#pragma once

#include <svanes/render/texture_manager.hpp>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Surface;

namespace svanes::internal {

/**
 * Internal interface for the TextureManager class.
 * This class provides methods for creating a TextureManager and resolving
 * texture handles to SDL_Texture pointers. As it deals with raw SDL_Texture
 * pointers, it should not be exposed to game code. The TextureManager class
 * itself is responsible for managing the lifetime of SDL_Texture objects. The
 * TextureManagerInternal class is a friend of TextureManager, allowing it to
 * access private members such as the textures map and the SDL_Renderer pointer.
 */
class TextureManagerInternal final {
public:
    /**
     * Creates a texture from an SDL_Surface and stores it in the manager.
     *
     * @param texture_manager The TextureManager instance to use for creating
     * the texture.
     * @param surface The SDL_Surface to create the texture from.
     *
     * @return A TextureHandle that can be used to reference the created
     * texture.
     *
     * @throws std::runtime_error if the texture cannot be created.
     */
    static TextureHandle CreateFromSurface(TextureManager &texture_manager,
                                           SDL_Surface *surface);

    /**
     * Destroys a texture associated with the given handle in the provided
     * TextureManager. This should not be exposed to game code, as it directly
     * manipulates the internal state of the TextureManager.
     *
     * @param texture_manager The TextureManager instance that owns the texture
     * to be destroyed.
     * @param handle The TextureHandle of the texture to destroy.
     *
     * @throws std::invalid_argument if the handle is invalid or does not exist.
     */
    static void Destroy(TextureManager &texture_manager, TextureHandle handle);

    /**
     * Creates a TextureManager instance using the provided SDL_Renderer.
     * Throws an exception if the renderer is null.
     * @param renderer The SDL_Renderer to use for texture management.
     * @return A TextureManager instance.
     * @throws std::invalid_argument if the renderer is null.
     */
    static TextureManager Create(SDL_Renderer *renderer);

    /**
     * Resolves a texture handle to its corresponding SDL_Texture pointer using
     * the provided TextureManager. This should not be exposed to game code, as
     * it returns a raw pointer to an SDL_Texture. Throws an exception if the
     * handle is invalid or does not exist in the manager.
     * @param texture_manager The TextureManager instance to use for resolving
     * the handle.
     * @param handle The TextureHandle to resolve.
     * @return A pointer to the SDL_Texture associated with the handle.
     * @throws std::invalid_argument if the handle is invalid or does not exist.
     */
    static SDL_Texture *Resolve(const TextureManager &texture_manager,
                                TextureHandle handle);
};

/**
 * Owns the lifetime of one texture stored in a particular TextureManager.
 * Destroying this wrapper releases the texture through that same manager.
 * Moving it transfers ownership and leaves the source empty.
 * That manager must outlive the wrapper.
 */
class OwnedTexture final {
public:
    /**
     * Constructs an OwnedTexture that manages the lifetime of a texture in the
     * specified TextureManager.
     *
     * @param manager The TextureManager instance that owns the texture.
     * @param handle The TextureHandle of the texture to manage.
     */
    OwnedTexture(TextureManager &manager, TextureHandle handle) noexcept;

    /**
     * Destroys the OwnedTexture and releases the associated texture through the
     * TextureManager.
     */
    ~OwnedTexture();

    /**
     * Override of the copy constructor to prevent copying of OwnedTexture
     * instances. This prevents something like:
     *
     * OwnedTexture t1(...);
     * OwnedTexture t2 = t1;
     */
    OwnedTexture(const OwnedTexture &) = delete;

    /**
     * Override of the copy assignment operator to prevent copying of
     * OwnedTexture instances. This prevents something like:
     *
     * OwnedTexture t1(...);
     * OwnedTexture t2(...);
     * t2 = t1;
     */
    OwnedTexture &operator=(const OwnedTexture &) = delete;

    /**
     * Move constructor for transferring ownership of the texture from another
     * OwnedTexture instance. After the move, the source instance will be left
     * empty.
     *
     * @param other The other OwnedTexture instance to move from.
     */
    OwnedTexture(OwnedTexture &&other) noexcept;

    /**
     * Move assignment operator for transferring ownership of the texture from
     * another OwnedTexture instance. After the move, the source instance will
     * be left empty. For example:
     *
     * OwnedTexture t1(...);
     * OwnedTexture t2(...);
     * t2 = std::move(t1); // t2 now owns the texture, and t1 is empty.
     *
     * @param other The other OwnedTexture instance to move from.
     *
     * @return A reference to this OwnedTexture instance after the move.
     */
    OwnedTexture &operator=(OwnedTexture &&other) noexcept;

    /**
     * Returns the TextureHandle associated with this OwnedTexture.
     *
     * @return The TextureHandle of the managed texture.
     */
    TextureHandle GetHandle() const;

private:
    /**
     * Resets the OwnedTexture, releasing the associated texture through the
     * TextureManager and leaving the wrapper empty.
     */
    void Reset() noexcept;

    // The TextureManager instance who owns the texture.
    TextureManager *manager;

    // The handle of the texture being managed who only exists in the manager.
    TextureHandle handle;
};

} // namespace svanes::internal
