#include <svanes/utility/file_io.hpp>

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace svanes {

std::vector<std::byte> ReadFile(const std::filesystem::path &path) {

    // Open the file in binary mode and seek to the end to determine its size
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    const auto size = file.tellg();

    // make sure the file size is valid and not too large to read into memory
    if (size < 0 ||
        static_cast<std::uint64_t>(size) >
            std::numeric_limits<std::size_t>::max() ||
        static_cast<std::uint64_t>(size) >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error("File size is invalid: " + path.string());
    }

    // container for the file contents, sized to the file's size
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));

    // Seek back to the beginning and read the file contents into the vector
    file.seekg(0);
    if (!bytes.empty()) {
        file.read(reinterpret_cast<char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }

    // read could have failed but not thrown so we need to check the stream
    // state after reading
    if (!file) {
        throw std::runtime_error("Could not read file: " + path.string());
    }

    return bytes;
}

void WriteFile(const std::filesystem::path &path,
               std::span<const std::byte> bytes) {


    if (bytes.size() > static_cast<std::uint64_t>(
                           std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error("File is too large to write: " + path.string());
    }

    // Open the file in binary mode, truncating it if it exists.
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    if (!bytes.empty()) {
        file.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }

    // rather than let raii handle closing the file, we explicitly close it here
    // so we can check for errors
    file.close();
    if (!file) {
        throw std::runtime_error("Could not write file: " + path.string());
    }
}

} // namespace svanes
