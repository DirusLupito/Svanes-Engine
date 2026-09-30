#include "missile_serialization.hpp"

#include "dynamic_object_serialization.hpp"
#include "json.hpp"

#include <stdexcept>

/**
 * Reads the missile's shared geometry and its health and explosion properties.
 *
 * @param json The JSON object representing the missile.
 * @return The validated missile definition.
 */
static MissileDefinition ReadMissile(const nlohmann::json &json) {
    MissileDefinition definition{
        ReadDynamicObject(json), ReadFloat(json.at("health")),
        ReadFloat(json.at("damage")), ReadFloat(json.at("blast_radius")),
        svanes::SecondsToTics(ReadFloat(json.at("arming_seconds")))};

    ValidateMissile(definition);
    return definition;
}

/**
 * Writes a missile definition, including the damage and radius of its
 * explosion.
 *
 * @param definition The missile definition to write.
 * @return The JSON object representing the missile.
 */
static nlohmann::json WriteMissile(const MissileDefinition &definition) {
    ValidateMissile(definition);
    auto json = WriteDynamicObject(definition.object);

    json["health"] = definition.health;
    json["damage"] = definition.damage;
    json["blast_radius"] = definition.blast_radius;
    json["arming_seconds"] =
        static_cast<double>(definition.arming_tics) / svanes::TicsPerSecond;

    return json;
}

MissileDefinition DeserializeMissile(std::string_view text) {
    return ReadMissile(nlohmann::json::parse(text));
}

std::string SerializeMissile(const MissileDefinition &definition) {
    return WriteMissile(definition).dump(2) + '\n';
}

MissileDefinition LoadMissile(const std::filesystem::path &path) {
    const auto json = ReadJson(path);
    try {
        return ReadMissile(json);
    } catch (const std::exception &error) {
        throw std::invalid_argument(path.string() + ": " + error.what());
    }
}

void SaveMissile(const std::filesystem::path &path,
                 const MissileDefinition &definition) {
    WriteJson(path, WriteMissile(definition));
}
