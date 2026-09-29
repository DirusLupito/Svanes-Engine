#include "json.hpp"

#include <fstream>
#include <stdexcept>

nlohmann::json ReadJson(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Could not open JSON file: " + path.string());
    }

    try {
        auto value = nlohmann::json::parse(stream);
        if (stream.bad()) {
            throw std::runtime_error("Could not read JSON file");
        }
        return value;
    } catch (const std::exception &error) {
        throw std::runtime_error(path.string() + ": " + error.what());
    }
}

void WriteJson(const std::filesystem::path &path, const nlohmann::json &value) {

    // Write the JSON value to the file with pretty printing (2 spaces
    // indentation), and a newline at the end of the file.
    const std::string text = value.dump(2) + '\n';
    std::ofstream stream(path, std::ios::binary);
    stream << text;
    stream.close();
    if (!stream) {
        throw std::runtime_error("Could not write JSON file: " + path.string());
    }
}
