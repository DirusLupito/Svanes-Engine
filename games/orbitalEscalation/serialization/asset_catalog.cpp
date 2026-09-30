#include "asset_catalog.hpp"

#include "dynamic_object_serialization.hpp"
#include "missile_serialization.hpp"
#include "ship_serialization.hpp"

#include <algorithm>
#include <stdexcept>

/**
 * Finds the JSON files in an asset folder, including its subfolders.
 *
 * @param folder The folder to search. A missing folder contains no definitions.
 * @return The paths of the JSON files in the folder.
 */
static std::vector<std::filesystem::path>
DefinitionFiles(const std::filesystem::path &folder) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(folder)) {
        return files;
    }

    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(folder)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }

    // Sorting the files makes the loading order consistent, although attachment
    // references do not depend on that order.
    std::sort(files.begin(), files.end());
    return files;
}

AssetCatalog::AssetCatalog(const std::filesystem::path &root) {
    if (!std::filesystem::is_directory(root)) {
        throw std::invalid_argument("Asset folder does not exist: " +
                                    root.string());
    }

    // Load ships
    for (const auto &path : DefinitionFiles(root / "ships")) {
        auto definition = LoadShip(path);
        const auto name = definition.object.name;
        if (!ships.emplace(name, std::move(definition)).second) {
            throw std::invalid_argument(path.string() +
                                        ": Duplicate ship name: " + name);
        }
    }

    // Load missiles
    for (const auto &path : DefinitionFiles(root / "missiles")) {
        auto definition = LoadMissile(path);
        const auto name = definition.object.name;
        if (!missiles.emplace(name, std::move(definition)).second) {
            throw std::invalid_argument(path.string() +
                                        ": Duplicate missile name: " + name);
        }
    }

    // Now that all definitions are loaded, we can check their references.
    // In particular, ships can refer to missiles even though we loaded all of
    // the ships before loading any of the missiles.

    std::map<std::pair<DynamicObjectType, std::string>, Visit> visits;

    // Ship validation
    for (const auto &[name, definition] : ships) {
        ValidateReferences(DynamicObjectType::Ship, name, visits);
    }

    // Missile validation
    for (const auto &[name, definition] : missiles) {
        ValidateReferences(DynamicObjectType::Missile, name, visits);
    }
}

const DynamicObjectDefinition &
AssetCatalog::GetDefinition(DynamicObjectType type,
                            const std::string &name) const {
    switch (type) {
    case DynamicObjectType::Ship:
        if (const auto found = ships.find(name); found != ships.end()) {
            return found->second.object;
        }
        break;
    case DynamicObjectType::Missile:
        if (const auto found = missiles.find(name); found != missiles.end()) {
            return found->second.object;
        }
        break;
    }

    throw std::invalid_argument("Missing " + WriteDynamicObjectType(type) +
                                " definition: " + name);
}

void AssetCatalog::ValidateReferences(
    DynamicObjectType type, const std::string &name,
    std::map<std::pair<DynamicObjectType, std::string>, Visit> &visits) const {
    // Names do not uniquely identify definitions, however, given a type,
    // the name must be unique. We cannot have two basic_missiles missiles, for
    // example.
    const auto key = std::make_pair(type, name);

    // If we have already visited this definition, we can skip it. If we are
    // still visiting it, we have a cycle.
    if (const auto found = visits.find(key); found != visits.end()) {
        if (found->second == Visit::Visiting) {
            throw std::invalid_argument("Attachment cycle at " +
                                        WriteDynamicObjectType(type) + ": " +
                                        name);
        }

        // Must be Visit::Complete, so we have already checked this definition
        // and its attachments.
        return;
    }

    // Must've never been visited, so we need to check its attachments.

    const auto &definition = GetDefinition(type, name);
    visits.emplace(key, Visit::Visiting);
    for (const auto &attachment : definition.attachments) {
        try {
            // Recursive case: check the attachment's definition and its
            // attachments.
            ValidateReferences(attachment.type, attachment.name, visits);
        } catch (const std::invalid_argument &error) {
            // extra debug details
            throw std::invalid_argument(WriteDynamicObjectType(type) + " '" +
                                        name + "': " + error.what());
        }
    }

    // Base case: everything checks out/this definition either has no
    // attachments or all of its attachments are valid.
    visits.at(key) = Visit::Complete;
}

void AssetCatalog::CreateAttachments(
    svanes::Registry &world, svanes::Entity gameplay_timeline,
    DynamicObject &object, const DynamicObjectDefinition &definition) const {
    for (const auto &attachment : definition.attachments) {
        object.AddAttachment(
            world,
            Create(world, gameplay_timeline, attachment.type, attachment.name),
            attachment.transform);
    }
}

std::unique_ptr<Ship> AssetCatalog::CreateShip(svanes::Registry &world,
                                               svanes::Entity gameplay_timeline,
                                               const std::string &name) const {
    const auto &definition = GetDefinition(DynamicObjectType::Ship, name);
    auto ship =
        std::make_unique<Ship>(world, gameplay_timeline, ships.at(name));
    CreateAttachments(world, gameplay_timeline, *ship, definition);
    return ship;
}

std::unique_ptr<DynamicObject>
AssetCatalog::Create(svanes::Registry &world, svanes::Entity gameplay_timeline,
                     DynamicObjectType type, const std::string &name) const {

    // For now we're either making a ship or a missile.
    if (type == DynamicObjectType::Ship) {
        return CreateShip(world, gameplay_timeline, name);
    }

    // TODO: put this into a new CreateMissile function, similar to CreateShip
    const auto &definition = GetDefinition(type, name);
    auto missile =
        std::make_unique<Missile>(world, gameplay_timeline, missiles.at(name));
    CreateAttachments(world, gameplay_timeline, *missile, definition);
    return missile;
}
