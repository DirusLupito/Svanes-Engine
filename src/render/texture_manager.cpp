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

void TextureManager::TextureDeleter::operator()(SDL_Texture* texture) const
{
    SDL_DestroyTexture(texture);
}

TextureManager::TextureManager(SDL_Renderer* renderer)
    : renderer(renderer)
{
}

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

TextureManager::~TextureManager() = default;

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

TextureManager internal::TextureManagerInternal::Create(SDL_Renderer* renderer)
{
    if (renderer == nullptr) {
        throw std::invalid_argument("Texture manager requires a renderer.");
    }

    return TextureManager{renderer};
}

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
