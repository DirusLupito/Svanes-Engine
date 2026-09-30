#pragma once

#include "../game_objects/dynamic_object/missile.hpp"
#include "../game_objects/dynamic_object/ship.hpp"

#include <filesystem>
#include <map>
#include <utility>

/**
 * Loads the game's dynamic object definitions and creates objects from them.
 * All definitions are loaded before attachment references are checked,
 * so for example, a ship can refer to a missile whose file happens to be loaded
 * later.
 *
 * The catalog owns definitions, not entities. Each call to Create constructs
 * separate entities, even when several attachments use the same definition.
 * Created objects do not refer back to the catalog and can outlive it.
 */
class AssetCatalog final {
public:
    /**
     * Loads the JSON files in the ships and missiles folders underneath root,
     * including any subfolders.
     *
     * @param root The folder containing the ships and missiles folders.
     *
     * @throws std::invalid_argument If definitions are invalid, names are
     * duplicated within a type, references are missing, or attachments form a
     * cycle. Cycles would otherwise create attachments forever.
     */
    explicit AssetCatalog(const std::filesystem::path &root);

    /**
     * Creates a ship and its attachments from a named definition.
     *
     * @param world The registry in which to create the entities.
     * @param gameplay_timeline The parent timeline for all created objects.
     * @param name The name of the ship definition.
     * @return The new ship, which owns its attached objects.
     * @throws std::invalid_argument If the ship definition does not exist.
     */
    std::unique_ptr<Ship> CreateShip(svanes::Registry &world,
                                     svanes::Entity gameplay_timeline,
                                     const std::string &name) const;

    /**
     * Creates a dynamic object and its attachments from a typed name. This is
     * used for attachments because an attachment can be either a ship or a
     * missile.
     *
     * @param world The registry in which to create the entities.
     * @param gameplay_timeline The parent timeline for all created objects.
     * @param type The type of definition to look up.
     * @param name The name of the definition within that type.
     *
     * @return The new object, which owns its attached objects.
     *
     * @throws std::invalid_argument If the definition does not exist.
     */
    std::unique_ptr<DynamicObject> Create(svanes::Registry &world,
                                          svanes::Entity gameplay_timeline,
                                          DynamicObjectType type,
                                          const std::string &name) const;

private:
    /**
     * Represents the visitation state of a definition while checking
     * attachments.
     *
     * MEMBERS:
     * - Visiting: The definition is still having its attachments checked.
     * - Complete: The definition and all definitions it attaches have been
     * checked.
     */
    enum class Visit { Visiting, Complete };

    // Names only have to be unique within their type. A ship and a missile
    // with the same name are two different definitions.

    // The ship definitions loaded from JSON files.
    std::map<std::string, ShipDefinition> ships;

    // The missile definitions loaded from JSON files.
    std::map<std::string, MissileDefinition> missiles;

    /**
     * Finds the common properties of a definition, regardless of its type.
     *
     * @param type The type of definition to look up.
     * @param name The name of the definition within that type.
     *
     * @return The definition's common dynamic object properties.
     *
     * @throws std::invalid_argument If the definition does not exist.
     */
    const DynamicObjectDefinition &GetDefinition(DynamicObjectType type,
                                                 const std::string &name) const;

    /**
     * Checks a definition's attachments recursively. Revisiting a definition
     * still being checked means there is a cycle. Revisiting a completed one
     * just means another attachment uses the same definition, which is fine.
     *
     * @param type The type of definition to check.
     * @param name The name of the definition within that type.
     * @param visits The definitions visited during this validation pass.
     */
    void ValidateReferences(DynamicObjectType type, const std::string &name,
                            std::map<std::pair<DynamicObjectType, std::string>,
                                     Visit> &visits) const;

    /**
     * Creates and attaches the objects referred to by a definition.
     *
     * @param world The registry containing the parent object.
     * @param gameplay_timeline The parent timeline for the new objects.
     * @param object The object that will own the attachments.
     * @param definition The definition containing the attachment references.
     */
    void CreateAttachments(svanes::Registry &world,
                           svanes::Entity gameplay_timeline,
                           DynamicObject &object,
                           const DynamicObjectDefinition &definition) const;
};
