#include "enemy.hpp"

#include "bullets.hpp"
#include "minions.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPathCenterX = 1600.0F;
constexpr float kPathY = 300.0F;
constexpr float kPathHalfWidth = 700.0F;
constexpr double kPathRadiansPerSecond = 0.6;
constexpr float kMoveSpeed = 700.0F;
constexpr float kBulletSpeed = 600.0F;
const svanes::TicCount kFireInterval = svanes::SecondsToTics(0.8);
constexpr std::uint32_t kMinGroupSize = 3;
constexpr std::uint32_t kMaxGroupSize = 5;
constexpr double kMinSpawnSeconds = 20.0;
constexpr double kMaxSpawnSeconds = 40.0;

} // namespace

Enemy::Enemy(svanes::Registry &world, svanes::Entity clock)
    : entity(world.CreateEntity()), health(EnemyMaxHealth),
      random(std::random_device{}()), spawn_cooldown(RollSpawnDelay()) {
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = kPathCenterX, .y = kPathY});
    world.AddComponent<svanes::Timeline>(entity, svanes::Timeline{clock});
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{
                    svanes::Rectangle2D{.width = EnemySize, .height = EnemySize}});
}

void Enemy::Advance(svanes::Registry &world, bool spawning) {
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

    if (!spawning) {
        return;
    }

    spawn_cooldown -= std::min(spawn_cooldown, delta_tics);
    if (spawn_cooldown == 0) {
        const std::uint32_t group_size =
            std::uniform_int_distribution<std::uint32_t>{
                kMinGroupSize, kMaxGroupSize}(random);
        const std::uint32_t archers = group_size / 2;
        std::uniform_real_distribution<float> spread{-EnemySize * 0.5F,
                                                     EnemySize * 0.5F};
        for (std::uint32_t index = 0; index < group_size; ++index) {
            const MinionKind kind =
                index < archers ? MinionKind::Archer : MinionKind::Melee;
            SpawnMinion(world, entity, kind,
                        {transform.x + spread(random), transform.y});
        }
        spawn_cooldown = RollSpawnDelay();
    }
}

void Enemy::Hit(svanes::Registry &world) {
    if (!IsAlive()) {
        return;
    }

    --health;
    if (health == 0) {
        world.RemoveComponent<svanes::Collider2D>(entity);
    }
}

bool Enemy::IsAlive() const { return health > 0; }

std::uint32_t Enemy::GetHealth() const { return health; }

svanes::Entity Enemy::GetEntity() const { return entity; }

svanes::TicCount Enemy::RollSpawnDelay() {
    return svanes::SecondsToTics(std::uniform_real_distribution<double>{
        kMinSpawnSeconds, kMaxSpawnSeconds}(random));
}
