#include "json.hpp"

#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

using nlohmann::json;

json ReadJson(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Could not open JSON file: " + path.string());
    }

    try {
        auto value = json::parse(stream);
        if (stream.bad()) {
            throw std::runtime_error("Could not read JSON file");
        }
        return value;
    } catch (const std::exception &error) {
        throw std::runtime_error(path.string() + ": " + error.what());
    }
}

void WriteJson(const std::filesystem::path &path, const json &value) {

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

// the json library already reads arrays, floating point numbers, etc just fine.
// These functions are just for validating that the values are within the
// expected ranges and types.

float ReadFloat(const json &value) {
    if (!value.is_number()) {
        throw std::invalid_argument("JSON value must be a number");
    }

    const double number = value.get<double>();
    if (!std::isfinite(number) ||
        std::abs(number) > std::numeric_limits<float>::max()) {
        throw std::invalid_argument("JSON number must fit a finite float");
    }

    return static_cast<float>(number);
}

const json::array_t &ReadArray(const json &value, std::size_t size) {
    const auto &array = value.get_ref<const json::array_t &>();
    if (array.size() != size) {
        throw std::invalid_argument("Expected array of " +
                                    std::to_string(size) + " elements");
    }
    return array;
}

std::int64_t ReadInt64(const json &value) {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned() &&
         value.get<std::uint64_t>() >
             static_cast<std::uint64_t>(
                 std::numeric_limits<std::int64_t>::max()))) {
        throw std::invalid_argument(
            "JSON value must fit a signed 64-bit integer");
    }

    return value.get<std::int64_t>();
}

std::uint8_t ReadUInt8(const json &value) {
    const auto integer = ReadInt64(value);

    if (integer < 0 || integer > 255) {
        throw std::invalid_argument(
            "JSON value must fit an unsigned 8-bit integer (0 to 255)");
    }

    return static_cast<std::uint8_t>(integer);
}
