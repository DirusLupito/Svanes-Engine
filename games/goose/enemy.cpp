#include "enemy.hpp"

#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPathCenterX = 1600.0F;
constexpr float kPathY = 300.0F;
constexpr float kPathHalfWidth = 700.0F;
constexpr double kPathRadiansPerSecond = 0.8;
constexpr float kMoveSpeed = 700.0F;
constexpr float kBulletSpeed = 600.0F;
constexpr std::uint32_t kHits = 10;
const svanes::TicCount kFireInterval = svanes::SecondsToTics(0.8);

} // namespace

Enemy::Enemy(svanes::Registry &world, svanes::Entity clock)
    : entity(world.CreateEntity()), hits_remaining(kHits) {
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = kPathCenterX, .y = kPathY});
    world.AddComponent<svanes::Timeline>(entity, svanes::Timeline{clock});
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{
                    svanes::Rectangle2D{.width = EnemySize, .height = EnemySize}});
}

void Enemy::Advance(svanes::Registry &world) {
    const svanes::Timeline &timeline =
        world.GetComponent<svanes::Timeline>(entity);
    const svanes::TicCount delta_tics = timeline.GetDeltaTics();
    if (!IsAlive() || delta_tics == 0) {
        return;
    }

    const double seconds = static_cast<double>(timeline.GetTotalTics()) /
                           static_cast<double>(svanes::TicsPerSecond);
    const float target_x =
        kPathCenterX +
        static_cast<float>(std::sin(seconds * kPathRadiansPerSecond)) *
            kPathHalfWidth;

    svanes::Transform &transform = world.GetComponent<svanes::Transform>(entity);
    const float step = kMoveSpeed * static_cast<float>(delta_tics) /
                       static_cast<float>(svanes::TicsPerSecond);
    const float offset = target_x - transform.x;
    transform.x += std::clamp(offset, -step, step);
    transform.y = kPathY;

    fire_cooldown -= std::min(fire_cooldown, delta_tics);
    if (fire_cooldown == 0) {
        SpawnBullet(world, entity, {transform.x, transform.y}, {0.0F, 1.0F},
                    kBulletSpeed);
        fire_cooldown = kFireInterval;
    }
}

void Enemy::Hit(svanes::Registry &world) {
    if (!IsAlive()) {
        return;
    }

    --hits_remaining;
    if (hits_remaining == 0) {
        world.RemoveComponent<svanes::Collider2D>(entity);
    }
}

bool Enemy::IsAlive() const { return hits_remaining > 0; }

svanes::Entity Enemy::GetEntity() const { return entity; }
