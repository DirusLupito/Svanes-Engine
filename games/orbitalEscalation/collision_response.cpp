#include "collision_response.hpp"

#include "attachment_system.hpp"
#include "combat/missile_system.hpp"

#include <svanes/events/collision_event.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>

#include <memory>

constexpr float kCollisionAcceleration = 400000.0F;

void RegisterPhysicalCollisionHandler(svanes::Registry &world,
                                      svanes::EventManager &events) {
    events.Subscribe(
        svanes::CollisionEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<svanes::CollisionEvent>>(
            [&world](const svanes::CollisionEvent &event) {
                const auto &pair = event.collision;
                if (GetAttachmentRoot(world, pair.a) ==
                        GetAttachmentRoot(world, pair.b) ||
                    IgnoresFiringShip(world, pair.a, pair.b) ||
                    IgnoresFiringShip(world, pair.b, pair.a)) {
                    return;
                }

                // For each contact in the collision, apply an acceleration to
                // both entities in opposite directions along the contact
                // normal.
                for (const auto &collision : pair.collisions) {
                    const auto acceleration =
                        collision.normal *
                        svanes::PerSecondSquaredToPerTicSquared(
                            kCollisionAcceleration);

                    if (world.HasComponent<svanes::Kinematic2D>(pair.a)) {
                        auto &motion =
                            world.GetComponent<svanes::Kinematic2D>(pair.a);
                        motion.acceleration_x += acceleration.x;
                        motion.acceleration_y += acceleration.y;
                    }

                    if (world.HasComponent<svanes::Kinematic2D>(pair.b)) {
                        auto &motion =
                            world.GetComponent<svanes::Kinematic2D>(pair.b);
                        motion.acceleration_x -= acceleration.x;
                        motion.acceleration_y -= acceleration.y;
                    }
                }
            }));
}
