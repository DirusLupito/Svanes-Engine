#include "missile_serialization.hpp"

#include "dynamic_object_serialization.hpp"
#include "json.hpp"

#include <stdexcept>

MissileDefinition DeserializeMissile(std::string_view text) {
    return {ReadDynamicObject(nlohmann::json::parse(text))};
}

std::string SerializeMissile(const MissileDefinition &definition) {
    return WriteDynamicObject(definition.object).dump(2) + '\n';
}

MissileDefinition LoadMissile(const std::filesystem::path &path) {
    const auto json = ReadJson(path);
    try {
        return {ReadDynamicObject(json)};
    } catch (const std::exception &error) {
        throw std::invalid_argument(path.string() + ": " + error.what());
    }
}

void SaveMissile(const std::filesystem::path &path,
                 const MissileDefinition &definition) {
    WriteJson(path, WriteDynamicObject(definition.object));
}
