/**
 * Manages the lifecycle of textures in the rendering system, 
 * providing functionality to load textures from files and create textures from raw image data.
 * @file texture_manager.cpp
 */

#include "texture_manager_internal.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace svanes {

/**
 * Used by std::unique_ptr to automatically manage the lifetime of SDL_Texture objects.
 * Destroying a texture is simple, as the only resource that needs to be freed is the SDL_Texture itself.
 */

// We define the call operator () to specify how to delete an SDL_Texture.
// Essentially this means we can say deleter(texture) which is equivalent to
// deleter.operator()(texture)
// Alternatively, we could have used a function pointer and decltype the 
// function pointer type, but then every time we wanted to make a unique_ptr
// we would have to give it the exact delete function.
// Whereas here by passing in a struct which has a call operator, the unique_ptr
// code will already try to call the operator() on the deleter.
// For the function pointer, that tries to call the function pointed to,
// and for the struct, it tries to call the operator() on the struct.
void TextureManager::TextureDeleter::operator()(SDL_Texture* texture) const
{
    SDL_DestroyTexture(texture);
}

TextureManager::TextureManager(SDL_Renderer* renderer)
    : renderer(renderer)
{
}

/**
 * Calculates the expected number of pixels in an image based on its width and height.
 * Throws an exception if the width or height is non-positive or if the calculated pixel count exceeds the maximum size of std::size_t.
 * @param image The image data for which to calculate the expected pixel count.
 * @return The expected number of pixels in the image.
 * @throws std::invalid_argument if the image dimensions are invalid or too large.
 */
std::size_t TextureManager::GetExpectedPixelCount(const ImageData& image)
{
    if (image.width <= 0 || image.height <= 0) {
        throw std::invalid_argument("Image dimensions must be greater than zero.");
    }

    constexpr std::size_t channel_count = 4;
    const auto width = static_cast<std::size_t>(image.width);
    const auto height = static_cast<std::size_t>(image.height);
    const auto maximum_size = std::numeric_limits<std::size_t>::max();

    if (width > maximum_size / height || width * height > maximum_size / channel_count) {
        throw std::invalid_argument("Image dimensions are too large.");
    }

    return width * height * channel_count;
}

/**
 * Stores a texture in the manager and returns a handle to it.
 * Throws an exception if the texture handle space is exhausted.
 * @param texture The unique pointer to the SDL_Texture to store.
 * @return A TextureHandle that can be used to reference the stored texture.
 * @throws std::runtime_error if the texture handle space is exhausted.
 */
TextureHandle TextureManager::Store(TexturePointer texture)
{
    if (next_id == 0) {
        throw std::runtime_error("Texture handle space is exhausted.");
    }

    const TextureHandle handle{next_id++};
    
    // Unique pointers cannot be copied. Thus, we cannot just put a 
    // copy of the unique pointer into the map.
    // Instead, we use std::move to transfer ownership of the unique pointer into the map.
    // std::move doesn't actually move anything, it just casts the unique pointer 
    // to something that can be moved from (if we ran textures[handle.id] = texture;
    // it would cause the compiler to error.

    textures[handle.id] = std::move(texture);
    return handle;
}

/**
 * Destructor for the TextureManager class. 
 * Currently, it does not need to perform any special cleanup,
 * as the unique pointers in the textures map will automatically 
 * clean up the SDL_Texture resources when the TextureManager is destroyed.
 */
TextureManager::~TextureManager() = default;

/**
 * Loads a texture from a file and stores it in the manager.
 * Throws an exception if the path is empty or if the texture cannot be loaded.
 * @param path The file path to the texture image.
 * @return A TextureHandle that can be used to reference the loaded texture.
 * @throws std::invalid_argument if the path is empty.
 * @throws std::runtime_error if the texture cannot be loaded.
 */
TextureHandle TextureManager::LoadTexture(std::string_view path)
{
    if (path.empty()) {
        throw std::invalid_argument("Texture path cannot be empty.");
    }

    const std::string path_string{path};
    TexturePointer texture{IMG_LoadTexture(renderer, path_string.c_str())};
    if (texture == nullptr) {
        throw std::runtime_error("Could not load texture '" + path_string + "': " + SDL_GetError());
    }

    // At this point we have a valid SDL_Texture, so we can delegate to Store.

    return Store(std::move(texture));
}

/**
 * Creates a texture from raw image data and stores it in the manager.
 * Throws an exception if the image data is invalid or if the texture cannot be created.
 * @param image The raw image data to create the texture from.
 * @return A TextureHandle that can be used to reference the created texture.
 * @throws std::invalid_argument if the image data is invalid.
 * @throws std::runtime_error if the texture cannot be created or if the pixel data cannot be uploaded.
 */
TextureHandle TextureManager::CreateTexture(const ImageData& image)
{
    const std::size_t expected_pixel_count = GetExpectedPixelCount(image);
    if (image.rgba_pixels.size() != expected_pixel_count) {
        throw std::invalid_argument("RGBA image data must contain exactly four bytes per pixel.");
    }

    if (image.width > std::numeric_limits<std::int32_t>::max() / 4) {
        throw std::invalid_argument("Image width is too large.");
    }

    TexturePointer texture{SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        image.width,
        image.height
    )};

    if (texture == nullptr) {
        throw std::runtime_error("Could not create texture: " + std::string{SDL_GetError()});
    }

    // Pitch is like stride, but for 2D images. It is the number of bytes in a row of pixel data
    // including any padding. For RGBA images, each pixel is 4 bytes, so the pitch is width * 4.

    const std::int32_t pitch = image.width * 4;
    if (!SDL_UpdateTexture(texture.get(), nullptr, image.rgba_pixels.data(), pitch)) {
        throw std::runtime_error("Could not upload texture data: " + std::string{SDL_GetError()});
    }

    // At this point we have a valid SDL_Texture, so we can delegate to Store.

    return Store(std::move(texture));
}

/**
 * Creates a TextureManager instance using the provided SDL_Renderer.
 * Throws an exception if the renderer is null.
 * @param renderer The SDL_Renderer to use for texture management.
 * @return A TextureManager instance.
 * @throws std::invalid_argument if the renderer is null.
 */
TextureManager internal::TextureManagerInternal::Create(SDL_Renderer* renderer)
{
    if (renderer == nullptr) {
        throw std::invalid_argument("Texture manager requires a renderer.");
    }

    return TextureManager{renderer};
}

/**
 * Resolves a texture handle to its corresponding SDL_Texture pointer using the provided TextureManager.
 * This should not be exposed to game code, as it returns a raw pointer to an SDL_Texture.
 * Throws an exception if the handle is invalid or does not exist in the manager.
 * @param texture_manager The TextureManager instance to use for resolving the handle.
 * @param handle The TextureHandle to resolve.
 * @return A pointer to the SDL_Texture associated with the handle.
 * @throws std::invalid_argument if the handle is invalid or does not exist.
 */
SDL_Texture* internal::TextureManagerInternal::Resolve(
    const TextureManager& texture_manager,
    TextureHandle handle
)
{
    if (handle.id == 0) {
        throw std::invalid_argument("Texture handle 0 is invalid.");
    }

    // .at() throws an exception if the key does not exist,
    // but we want to throw a more specific exception.
    // .find() returns an iterator to the element if it exists, or textures.end() if it does not.

    const auto texture = texture_manager.textures.find(handle.id);
    if (texture == texture_manager.textures.end()) {
        throw std::invalid_argument("Texture handle does not exist.");
    }

    // texture is an iterator to a std::pair<const std::uint32_t, TexturePointer>
    // So we only care about the second element of the pair, which is the unique pointer to the SDL_Texture.

    return texture->second.get();
}

}
