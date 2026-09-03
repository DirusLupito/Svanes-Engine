/**
 * Header file for the public interface of the TextureManager class.
 * Anything here can be used by external game code to manage textures.
 * @file texture_manager.hpp
 */

#pragma once

#include <svanes/render/basic_render_types.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>

struct SDL_Renderer;
struct SDL_Texture;

namespace svanes {

namespace internal {

class TextureManagerInternal;

}

/**
 * Manages textures for rendering, providing functionality to load textures from files
 * and create textures from raw image data.
 * The TextureManager class is responsible for managing the lifetime of SDL_Texture objects.
 * It provides methods to load textures from files and create textures from raw image data,
 * returning TextureHandle objects that can be used to reference the textures.
 * 
 * The TextureManager class is a friend of the TextureManagerInternal class,
 * allowing the internal class to access private members such as the textures map 
 * and the SDL_Renderer pointer.
 * 
 * Unlike its internal counterpart, the TextureManager class 
 * s intended to be used by external game code.
 */
class TextureManager final {
public:
    ~TextureManager();

    TextureHandle LoadTexture(std::string_view path);
    TextureHandle CreateTexture(const ImageData& image);

private:
    struct TextureDeleter {
        void operator()(SDL_Texture* texture) const;
    };

    // Unique pointers take two template parameters: the type of the object being managed and the deleter type.
    using TexturePointer = std::unique_ptr<SDL_Texture, TextureDeleter>;

    explicit TextureManager(SDL_Renderer* renderer);

    static std::size_t GetExpectedPixelCount(const ImageData& image);
    TextureHandle Store(TexturePointer texture);

    SDL_Renderer* renderer;
    std::uint32_t next_id = 1;

    // A hash map. Keys are texture IDs (uint32_t), and values are unique pointers to SDL_Texture objects.
    std::unordered_map<std::uint32_t, TexturePointer> textures;

    // allow the internal class to access private members of this class
    friend class internal::TextureManagerInternal;
};

}
