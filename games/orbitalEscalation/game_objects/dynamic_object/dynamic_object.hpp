#pragma once

#include "../visuals.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>

/**
 * Represents a dynamic object in the game world, consisting of some rendered
 * and collidable geometry, and a kinematic component for movement.
 */
class DynamicObject {
public:
    /**
     * Returns the unique identifier of the dynamic object entity.
     *
     * @return The unique identifier of the dynamic object entity.
     */
    svanes::Entity GetEntity() const;

    /**
     * Returns a reference to the Kinematic2D component of the dynamic object
     * entity.
     *
     * @param world The registry containing the dynamic object entity and its
     * components.
     * @return A reference to the Kinematic2D component of the dynamic object
     * entity.
     */
    svanes::Kinematic2D &GetKinematic(svanes::Registry &world) const;

    /**
     * Returns a reference to the Transform component of the dynamic object
     * entity.
     *
     * @param world The registry containing the dynamic object entity and its
     * components.
     * @return A reference to the Transform component of the dynamic object
     * entity.
     */
    svanes::Transform &GetTransform(svanes::Registry &world) const;

    /**
     * Updates the visuals of the dynamic object entity to match its current
     * transform.
     *
     * @param world The registry containing the dynamic object entity and its
     * components.
     */
    void UpdateVisuals(svanes::Registry &world) const;

    // Nobody should ever construct a DynamicObject directly, only derived
    // classes should be able to do that.
protected:
    /**
     * Constructs a DynamicObject entity in the provided registry.
     *
     * @param world The registry in which to create the dynamic object entity
     * and its components.
     * @param gameplay_timeline The timeline entity that controls the dynamic
     * object's behavior in the game world.
     * @param collider The Collider2D component that defines the dynamic
     * object's collision layer.
     * @param visuals A vector of Visual objects that define the dynamic
     * object's appearance in the game world.
     */
    DynamicObject(svanes::Registry &world, svanes::Entity gameplay_timeline,
                  svanes::Collider2D collider, std::vector<Visual> visuals);

    /**
     * Destroys the DynamicObject entity and removes all its associated
     * components from the registry.
     */
    ~DynamicObject() = default;

private:
    // The unique identifier of the dynamic object entity in the registry.
    svanes::Entity entity;

    // The Visuals object that manages the visual representation of the dynamic
    // object entity.
    Visuals visuals;
};
