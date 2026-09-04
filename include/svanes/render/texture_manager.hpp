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
    /**
     * Destructor for the TextureManager class. 
     * Currently, it does not need to perform any special cleanup,
     * as the unique pointers in the textures map will automatically 
     * clean up the SDL_Texture resources when the TextureManager is destroyed.
     */
    ~TextureManager();

    /**
     * Loads a texture from a file and stores it in the manager.
     * Throws an exception if the path is empty or if the texture cannot be loaded.
     * @param path The file path to the texture image.
     * @return A TextureHandle that can be used to reference the loaded texture.
     * @throws std::invalid_argument if the path is empty.
     * @throws std::runtime_error if the texture cannot be loaded.
     */
    TextureHandle LoadTexture(std::string_view path);

    

    /**
     * Creates a texture from raw image data and stores it in the manager.
     * Throws an exception if the image data is invalid or if the texture cannot be created.
     * @param image The raw image data to create the texture from.
     * @return A TextureHandle that can be used to reference the created texture.
     * @throws std::invalid_argument if the image data is invalid.
     * @throws std::runtime_error if the texture cannot be created or if the pixel data cannot be uploaded.
     */
    TextureHandle CreateTexture(const ImageData& image);

private:
    
    /**
     * Used by std::unique_ptr to automatically manage the lifetime of SDL_Texture objects.
     * Destroying a texture is simple, as the only resource that needs to be freed is the SDL_Texture itself.
     * 
     * We define the call operator () to specify how to delete an SDL_Texture.
     * Essentially this means we can say deleter(texture) which is equivalent to
     * deleter.operator()(texture)
     * Alternatively, we could have used a function pointer and decltype the 
     * function pointer type, but then every time we wanted to make a unique_ptr
     * we would have to give it the exact delete function.
     * Whereas here by passing in a struct which has a call operator, the unique_ptr
     * code will already try to call the operator() on the deleter.
     * For the function pointer, that tries to call the function pointed to,
     * and for the struct, it tries to call the operator() on the struct.
     */
    struct TextureDeleter {
        void operator()(SDL_Texture* texture) const;
    };

    // Unique pointers take two template parameters: the type of the object being managed and the deleter type.
    using TexturePointer = std::unique_ptr<SDL_Texture, TextureDeleter>;

    explicit TextureManager(SDL_Renderer* renderer);

    
    /**
     * Calculates the expected number of pixels in an image based on its width and height.
     * Throws an exception if the width or height is non-positive or if the calculated pixel count exceeds the maximum size of std::size_t.
     * @param image The image data for which to calculate the expected pixel count.
     * @return The expected number of pixels in the image.
     * @throws std::invalid_argument if the image dimensions are invalid or too large.
     */
    static std::size_t GetExpectedPixelCount(const ImageData& image);

    /**
     * Stores a texture in the manager and returns a handle to it.
     * Throws an exception if the texture handle space is exhausted.
     * @param texture The unique pointer to the SDL_Texture to store.
     * @return A TextureHandle that can be used to reference the stored texture.
     * @throws std::runtime_error if the texture handle space is exhausted.
     */
    TextureHandle Store(TexturePointer texture);

    SDL_Renderer* renderer;
    std::uint32_t next_id = 1;

    // A hash map. Keys are texture IDs (uint32_t), and values are unique pointers to SDL_Texture objects.
    std::unordered_map<std::uint32_t, TexturePointer> textures;

    // allow the internal class to access private members of this class
    friend class internal::TextureManagerInternal;
};

}
