#include "missile_launcher_serialization.hpp"

#include "dynamic_object_serialization.hpp"
#include "geometry_serialization.hpp"
#include <stdexcept>

/**
 * Reads a launcher's ammunition type, reload time, and launch properties.
 * Reload time is given in seconds and converted to local tics once here.
 *
 * @param json The JSON object representing the launcher.
 * @return The validated launcher definition.
 */
static MissileLauncherDefinition
ReadMissileLauncher(const nlohmann::json &json) {
    MissileLauncherDefinition definition{
        ReadDynamicObject(json), json.at("missile").get<std::string>(),
        svanes::SecondsToTics(ReadFloat(json.at("reload_seconds"))),
        ReadFloat(json.at("launch_speed")), ReadPoint(json.at("forward"))};

    ValidateMissileLauncher(definition);
    return definition;
}

/**
 * Writes a missile launcher definition.
 *
 * @param definition The missile launcher definition to write.
 * @return The JSON object representing the launcher.
 */
static nlohmann::json
WriteMissileLauncher(const MissileLauncherDefinition &definition) {
    ValidateMissileLauncher(definition);
    auto json = WriteDynamicObject(definition.object);

    json["missile"] = definition.missile;
    json["reload_seconds"] =
        static_cast<double>(definition.reload_tics) / svanes::TicsPerSecond;
    json["launch_speed"] = definition.launch_speed;
    json["forward"] = WritePoint(definition.forward);

    return json;
}

MissileLauncherDefinition DeserializeMissileLauncher(std::string_view text) {
    return ReadMissileLauncher(nlohmann::json::parse(text));
}

std::string
SerializeMissileLauncher(const MissileLauncherDefinition &definition) {
    return WriteMissileLauncher(definition).dump(2) + '\n';
}

MissileLauncherDefinition
LoadMissileLauncher(const std::filesystem::path &path) {
    const auto json = ReadJson(path);
    try {
        return ReadMissileLauncher(json);
    } catch (const std::exception &error) {
        throw std::invalid_argument(path.string() + ": " + error.what());
    }
}

void SaveMissileLauncher(const std::filesystem::path &path,
                         const MissileLauncherDefinition &definition) {
    WriteJson(path, WriteMissileLauncher(definition));
}
