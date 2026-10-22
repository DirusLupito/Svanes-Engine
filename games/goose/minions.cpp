#include "minions.hpp"

#include "arena.hpp"
#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

namespace {

constexpr float kWalkSpeed = 180.0F;
constexpr float kStopDistance = 2.0F;
constexpr float kArcherDistance = 400.0F;
constexpr float kArcherSlack = 20.0F;
constexpr float kArcherFireRange = 700.0F;
constexpr float kArcherBulletSpeed = 600.0F;
const svanes::TicCount kArcherFireInterval = svanes::SecondsToTics(1.5);

/**
 * A bullet an archer wants to fire, held until the minions are no longer being
 * iterated so creating it cannot disturb the iteration.
 *
 * FIELDS:
 * - owner: The entity the bullet is fired for.
 * - origin: The world position the bullet starts at.
 * - direction: The direction of travel. Not normalized.
 */
struct Shot {
    svanes::Entity owner;
    svanes::Vector2D origin;
    svanes::Vector2D direction;
};

/**
 * @param targets The positions to choose between.
 * @param from The position to measure from.
 * @return The target closest to the position, or empty if there are none.
 */
std::optional<svanes::Vector2D>
ClosestTarget(std::span<const svanes::Vector2D> targets,
              svanes::Vector2D from) {
    float closest_distance = std::numeric_limits<float>::infinity();
    std::optional<svanes::Vector2D> closest;
    for (const svanes::Vector2D target : targets) {
        const float distance = std::hypot(target.x - from.x, target.y - from.y);
        if (distance < closest_distance) {
            closest_distance = distance;
            closest = target;
        }
    }
    return closest;
}

/**
 * @param kind How a minion fights.
 * @param offset The target's horizontal offset from the minion.
 * @return The direction the minion should walk: -1 for left, +1 for right, 0
 * to stand still.
 */
float WalkDirection(MinionKind kind, float offset) {
    const float distance = std::abs(offset);
    const float toward = offset > 0.0F ? 1.0F : -1.0F;
    if (kind == MinionKind::Melee) {
        return distance > kStopDistance ? toward : 0.0F;
    }
    if (distance > kArcherDistance + kArcherSlack) {
        return toward;
    }
    if (distance < kArcherDistance - kArcherSlack) {
        return -toward;
    }
    return 0.0F;
}

/**
 * Pushes overlapping minions of the same kind sideways apart by their
 * colliders, splitting the push evenly between the two. Only horizontal, so
 * minions never stack. Minions of different kinds pass through each other, so
 * archers holding their distance do not block melee minions walking in.
 *
 * @param world The registry containing the minions.
 */
void SeparateMinions(svanes::Registry &world) {
    std::vector<svanes::Entity> minions;
    world.ForEach<Minion>([&](svanes::Entity entity, const Minion &) {
        minions.push_back(entity);
    });

    for (std::size_t first = 0; first < minions.size(); ++first) {
        for (std::size_t second = first + 1; second < minions.size();
             ++second) {
            if (world.GetComponent<Minion>(minions[first]).kind !=
                world.GetComponent<Minion>(minions[second]).kind) {
                continue;
            }

            svanes::Transform &a =
                world.GetComponent<svanes::Transform>(minions[first]);
            svanes::Transform &b =
                world.GetComponent<svanes::Transform>(minions[second]);
            const auto &a_body = std::get<svanes::Rectangle2D>(
                world.GetComponent<svanes::Collider2D>(minions[first])
                    .geometry);
            const auto &b_body = std::get<svanes::Rectangle2D>(
                world.GetComponent<svanes::Collider2D>(minions[second])
                    .geometry);

            const float overlap_x =
                (a_body.width + b_body.width) * 0.5F - std::abs(b.x - a.x);
            const float overlap_y =
                (a_body.height + b_body.height) * 0.5F - std::abs(b.y - a.y);
            if (overlap_x <= 0.0F || overlap_y <= 0.0F) {
                continue;
            }

            const float side = b.x >= a.x ? 1.0F : -1.0F;
            a.x -= side * overlap_x * 0.5F;
            b.x += side * overlap_x * 0.5F;
        }
    }
}

} // namespace

void SpawnMinion(svanes::Registry &world, svanes::Entity spawner,
                 MinionKind kind, svanes::Vector2D position) {
    const std::optional<svanes::Entity> clock =
        world.GetComponent<svanes::Timeline>(spawner).GetParent();
    const bool archer = kind == MinionKind::Archer;
    const float width = archer ? ArcherMinionWidth : MeleeMinionWidth;
    const float height = archer ? ArcherMinionHeight : MeleeMinionHeight;

    const svanes::Entity minion = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        minion, svanes::Transform{.x = position.x, .y = position.y});
    world.AddComponent<svanes::Timeline>(minion, svanes::Timeline{clock});
    world.AddComponent<svanes::Kinematic2D>(minion);
    world.AddComponent<svanes::Gravity>(minion);
    world.AddComponent<svanes::Collider2D>(
        minion, svanes::Collider2D{svanes::Rectangle2D{
                    .width = width + MinionSpacing, .height = height}});
    world.AddComponent<Minion>(
        minion, Minion{.spawner = spawner,
                       .kind = kind,
                       .fire_cooldown = archer ? kArcherFireInterval : 0});
}

void AdvanceMinions(svanes::Registry &world,
                    std::span<const svanes::Vector2D> targets) {
    std::vector<Shot> shots;

    world.ForEach<Minion, svanes::Transform, svanes::Kinematic2D,
                  svanes::Timeline>([&](svanes::Entity entity, Minion &minion,
                                        const svanes::Transform &transform,
                                        svanes::Kinematic2D &motion,
                                        const svanes::Timeline &timeline) {
        ResolveSolidCollisions(world, entity);

        const svanes::Vector2D position{transform.x, transform.y};
        const std::optional<svanes::Vector2D> target =
            ClosestTarget(targets, position);

        float direction = 0.0F;
        if (target) {
            direction = WalkDirection(minion.kind, target->x - position.x);
        }
        motion.velocity_x = svanes::PerSecondToPerTic(direction * kWalkSpeed);

        if (minion.kind != MinionKind::Archer) {
            return;
        }

        minion.fire_cooldown -=
            std::min(minion.fire_cooldown, timeline.GetDeltaTics());
        if (!target || minion.fire_cooldown > 0) {
            return;
        }

        const svanes::Vector2D aim{target->x - position.x,
                                   target->y - position.y};
        const float distance = std::hypot(aim.x, aim.y);
        if (distance > 0.0F && distance <= kArcherFireRange) {
            shots.push_back({minion.spawner, position, aim});
            minion.fire_cooldown = kArcherFireInterval;
        }
    });

    SeparateMinions(world);

    for (const Shot &shot : shots) {
        SpawnBullet(world, shot.owner, shot.origin, shot.direction,
                    kArcherBulletSpeed);
    }
}

void DamageMinion(svanes::Registry &world, svanes::Entity entity,
                  std::uint32_t damage) {
    Minion &minion = world.GetComponent<Minion>(entity);
    minion.health -= std::min(minion.health, damage);
    if (minion.health == 0) {
        world.DestroyEntity(entity);
    }
}
