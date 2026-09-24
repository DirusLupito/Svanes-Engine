#include "goose_simulation.hpp"

#include <svanes/game.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

void GooseSimulation::Initialize(svanes::GameContext& context,
                                  std::span<const svanes::PeerId> roster)
{
    if (!players.empty()) {
        throw std::logic_error("GooseSimulation has already been initialized.");
    }
    if (roster.empty()) {
        throw std::invalid_argument("GooseSimulation requires at least one player.");
    }
    for (std::size_t index = 0; index < roster.size(); ++index) {
        if (roster[index].value == 0 ||
            (index > 0 && roster[index - 1].value >= roster[index].value)) {
            throw std::invalid_argument("GooseSimulation requires nonzero peer ids in ascending, unique order.");
        }
    }
    context.automatic_simulation = false;
    players.reserve(roster.size());
    for (std::size_t index = 0; index < roster.size(); ++index) {
        players.push_back({roster[index], {}});
        const float x = 400.0F + 100.0F * static_cast<float>(index % 48);
        players.back().goose.Spawn(context, x, 700.0F);
    }
    enemy.Spawn(context.world, {1600.0F, 300.0F}, 120.0F, 10.0F);
}

void GooseSimulation::Step(svanes::Registry& world, svanes::Vector2D gravity,
                            std::span<const GooseIntent> inputs)
{
    if (players.empty()) {
        throw std::logic_error("GooseSimulation::Step called before Initialize.");
    }
    if (inputs.size() != players.size()) {
        throw std::invalid_argument("GooseSimulation requires one input per player.");
    }
    if (tick == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("GooseSimulation exhausted its tick counter.");
    }
    if (!std::isfinite(gravity.x) || !std::isfinite(gravity.y)) {
        throw std::invalid_argument("GooseSimulation requires finite gravity.");
    }
    for (const auto& input : inputs) {
        if ((input.move.x != -1.0F && input.move.x != 0.0F && input.move.x != 1.0F) ||
            input.move.y != 0.0F ||
            (input.dash != -1.0F && input.dash != 0.0F && input.dash != 1.0F) ||
            !std::isfinite(input.aim_point.x) || !std::isfinite(input.aim_point.y) ||
            std::abs(input.aim_point.x) > 100000.0F || std::abs(input.aim_point.y) > 100000.0F) {
            throw std::invalid_argument("GooseSimulation requires valid movement directions and finite, bounded aim.");
        }
    }

    std::vector<svanes::PhysicsTimeStep> steps;
    steps.reserve(players.size());
    for (const auto& player : players) {
        const auto entity = player.goose.GetEntity();
        auto& timeline = world.GetComponent<svanes::Timeline>(entity);
        timeline.Advance(GooseStepTics);
        steps.push_back({entity, timeline.GetDeltaTics()});
    }
    for (const auto entity : OrderedBullets(world)) {
        auto& timeline = world.GetComponent<svanes::Timeline>(entity);
        timeline.Advance(GooseStepTics);
        steps.push_back({entity, timeline.GetDeltaTics()});
    }
    svanes::AdvanceKinematics(world, gravity, physics_driver, steps);
    for (std::size_t index = 0; index < players.size(); ++index) {
        players[index].goose.Advance(world, inputs[index], steps[index].delta_tics);
    }
    const double elapsed_seconds = static_cast<double>(tick + 1) *
        static_cast<double>(GooseStepTics) / static_cast<double>(svanes::TicsPerSecond);
    EnemyIntent enemy_intent{};
    enemy_intent.move_to = {
        1600.0F + static_cast<float>(std::sin(elapsed_seconds * 0.8) * 700.0),
        300.0F
    };
    if (enemy.IsAlive()) {
        const auto position = enemy.Position(world);
        enemy_intent.fire = true;
        enemy_intent.aim_point = {position.x, position.y + 1000.0F};
        enemy.Advance(world, enemy_intent, GooseStepTics);
    }
    for (const auto entity : OrderedBullets(world)) {
        auto& bullet = world.GetComponent<Bullet>(entity);
        if (bullet.id != 0) {
            continue;
        }
        if (next_bullet_id == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("GooseSimulation exhausted its shot identifiers.");
        }
        bullet.id = next_bullet_id++;
        if (bullet.owner == enemy.GetEntity()) {
            bullet.owner_key = 0;
        } else {
            const auto owner = std::find_if(players.begin(), players.end(),
                [&](const auto& player) { return player.goose.GetEntity() == bullet.owner; });
            if (owner == players.end()) {
                throw std::logic_error("GooseSimulation found a bullet with an unknown owner.");
            }
            bullet.owner_key = owner->peer.value;
        }
    }
    const svanes::Rectangle2D bullet_bounds{3000.0F, -60.0F, 6600.0F, 2800.0F};
    for (const auto& hit : UpdateBullets(world, bullet_bounds)) {
        for (auto& player : players) {
            if (hit.target == player.goose.GetEntity()) {
                player.goose.ApplyKnockback(world, hit.direction, 350.0F);
                break;
            }
        }
        if (enemy.IsAlive() && hit.target == enemy.GetEntity()) {
            enemy.ApplyDamage(world, 1.0F);
        }
    }
    if (enemy.IsAlive()) {
        for (auto& player : players) {
            const auto entity = player.goose.GetEntity();
            const auto contact = svanes::DetectCollisions(
                world.GetComponent<svanes::Collider2D>(entity).geometry,
                world.GetComponent<svanes::Transform>(entity),
                world.GetComponent<svanes::Collider2D>(enemy.GetEntity()).geometry,
                world.GetComponent<svanes::Transform>(enemy.GetEntity()));
            if (!contact.empty()) {
                player.goose.ApplyKnockback(world, contact.front().normal, 550.0F);
            }
        }
    }
    svanes::AdvanceSpriteAnimations(world);
    ++tick;
}

GooseWorldSnapshot GooseSimulation::Capture(const svanes::Registry& world) const
{
    if (players.empty()) {
        throw std::logic_error("GooseSimulation::Capture called before Initialize.");
    }
    GooseWorldSnapshot snapshot{tick, {}, enemy.Capture(world), CaptureBullets(world), next_bullet_id};
    snapshot.geese.reserve(players.size());
    for (const auto& player : players) {
        snapshot.geese.push_back(player.goose.Capture(world));
    }
    return snapshot;
}

std::uint64_t GooseSimulation::Hash(const GooseWorldSnapshot& snapshot)
{
    svanes::MessageWriter writer;
    writer.WriteUint64(snapshot.tick);
    writer.WriteUint64(static_cast<std::uint64_t>(snapshot.geese.size()));
    for (const auto& goose : snapshot.geese) {
        writer.WriteFloat32(goose.transform.x);
        writer.WriteFloat32(goose.transform.y);
        writer.WriteFloat32(goose.transform.rotation);
        writer.WriteFloat32(goose.motion.velocity_x);
        writer.WriteFloat32(goose.motion.velocity_y);
        writer.WriteFloat32(goose.motion.acceleration_x);
        writer.WriteFloat32(goose.motion.acceleration_y);
        writer.WriteFloat32(goose.motion.angular_velocity);
        writer.WriteFloat32(goose.motion.angular_acceleration);
        for (const auto limit : {goose.motion.max_speed, goose.motion.max_acceleration,
                                goose.motion.max_angular_speed, goose.motion.max_angular_acceleration}) {
            writer.WriteBool(limit.has_value());
            if (limit) {
                writer.WriteFloat32(*limit);
            }
        }
        writer.WriteUint64(goose.timeline.GetTotalTics());
        writer.WriteUint64(goose.timeline.GetDeltaTics());
        writer.WriteBool(goose.timeline.IsPaused());
        writer.WriteBool(goose.grounded);
        writer.WriteUint64(goose.fly_time_remaining);
        writer.WriteUint64(goose.dash_timer);
        writer.WriteUint64(goose.dash_cooldown);
        writer.WriteUint64(goose.fire_cooldown);
        writer.WriteUint64(goose.knockback_timer);
        writer.WriteUint64(goose.invincible_timer);
    }
    writer.WriteFloat32(snapshot.enemy.transform.x);
    writer.WriteFloat32(snapshot.enemy.transform.y);
    writer.WriteFloat32(snapshot.enemy.transform.rotation);
    writer.WriteFloat32(snapshot.enemy.health.current);
    writer.WriteFloat32(snapshot.enemy.health.max);
    writer.WriteUint64(snapshot.enemy.fire_cooldown);
    writer.WriteBool(snapshot.enemy.alive);
    writer.WriteUint64(snapshot.next_bullet_id);
    writer.WriteUint64(static_cast<std::uint64_t>(snapshot.bullets.size()));
    for (const auto& shot : snapshot.bullets) {
        writer.WriteUint64(shot.bullet.id);
        writer.WriteUint32(shot.bullet.owner_key);
        writer.WriteFloat32(shot.transform.x);
        writer.WriteFloat32(shot.transform.y);
        writer.WriteFloat32(shot.transform.rotation);
        writer.WriteFloat32(shot.motion.velocity_x);
        writer.WriteFloat32(shot.motion.velocity_y);
        writer.WriteUint64(shot.timeline.GetTotalTics());
        writer.WriteUint64(shot.timeline.GetDeltaTics());
    }
    // FNV-1a operates on values, never struct padding or process-local handles.
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto byte : writer.Finish().bytes) {
        hash ^= std::to_integer<std::uint8_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void GooseSimulation::Restore(svanes::Registry& world, const GooseWorldSnapshot& snapshot)
{
    if (players.empty()) {
        throw std::logic_error("GooseSimulation::Restore called before Initialize.");
    }
    if (snapshot.geese.size() != players.size()) {
        throw std::invalid_argument("GooseSimulation snapshot does not match the player count.");
    }
    for (std::size_t index = 0; index < players.size(); ++index) {
        players[index].goose.Restore(world, snapshot.geese[index]);
    }
    enemy.Restore(world, snapshot.enemy);
    RestoreBullets(world, snapshot.bullets);
    next_bullet_id = snapshot.next_bullet_id;
    tick = snapshot.tick;
}

svanes::Entity GooseSimulation::PlayerEntity(svanes::PeerId peer) const
{
    for (const auto& player : players) {
        if (player.peer == peer) {
            return player.goose.GetEntity();
        }
    }
    throw std::invalid_argument("GooseSimulation: peer is outside the roster.");
}

std::uint64_t GooseSimulation::Tick() const
{
    return tick;
}
