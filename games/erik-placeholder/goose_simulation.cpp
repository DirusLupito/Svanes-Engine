#include "goose_simulation.hpp"

#include <svanes/game.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::array<float, 8> kSpawnX{400.0F, 550.0F, 700.0F, 850.0F, 1000.0F, 1150.0F, 1300.0F, 1450.0F};
constexpr float kSpawnY = 700.0F;
constexpr float kSpawnClearance = 80.0F;

void WriteTransform(svanes::MessageWriter& writer, const svanes::Transform& transform)
{
    writer.WriteFloat32(transform.x);
    writer.WriteFloat32(transform.y);
    writer.WriteFloat32(transform.rotation);
}

svanes::Transform ReadTransform(svanes::MessageReader& reader)
{
    svanes::Transform transform;
    transform.x = reader.ReadFloat32();
    transform.y = reader.ReadFloat32();
    transform.rotation = reader.ReadFloat32();
    return transform;
}

void WriteMotion(svanes::MessageWriter& writer, const svanes::Kinematic2D& motion)
{
    writer.WriteFloat32(motion.velocity_x);
    writer.WriteFloat32(motion.velocity_y);
    writer.WriteFloat32(motion.acceleration_x);
    writer.WriteFloat32(motion.acceleration_y);
    writer.WriteFloat32(motion.angular_velocity);
    writer.WriteFloat32(motion.angular_acceleration);
    for (const auto limit : {motion.max_speed, motion.max_acceleration,
                             motion.max_angular_speed, motion.max_angular_acceleration}) {
        writer.WriteBool(limit.has_value());
        if (limit) {
            writer.WriteFloat32(*limit);
        }
    }
}

svanes::Kinematic2D ReadMotion(svanes::MessageReader& reader)
{
    svanes::Kinematic2D motion;
    motion.velocity_x = reader.ReadFloat32();
    motion.velocity_y = reader.ReadFloat32();
    motion.acceleration_x = reader.ReadFloat32();
    motion.acceleration_y = reader.ReadFloat32();
    motion.angular_velocity = reader.ReadFloat32();
    motion.angular_acceleration = reader.ReadFloat32();
    for (auto* limit : {&motion.max_speed, &motion.max_acceleration,
                        &motion.max_angular_speed, &motion.max_angular_acceleration}) {
        if (reader.ReadBool()) {
            *limit = reader.ReadFloat32();
        }
    }
    return motion;
}

/**
 * Writes a timeline's elapsed time. Only root, running, unscaled timelines can be
 * rebuilt from their totals, which is all the simulation creates.
 * @param writer The message to append to.
 * @param timeline The timeline to encode.
 * @throws std::logic_error for a parented, paused, or scaled timeline.
 */
void WriteTimeline(svanes::MessageWriter& writer, const svanes::Timeline& timeline)
{
    const auto tic_size = timeline.GetTicSize();
    if (timeline.GetParent() || timeline.IsPaused() || tic_size.GetNumerator() != tic_size.GetDenominator()) {
        throw std::logic_error("GooseSimulation can only encode root, running, unscaled timelines.");
    }
    writer.WriteUint64(timeline.GetTotalTics());
    writer.WriteUint64(timeline.GetDeltaTics());
}

/**
 * Rebuilds a root, running, unscaled timeline with the given totals.
 * @param reader The message positioned at an encoded timeline.
 * @return A timeline reporting the same total and most recent delta.
 * @throws std::invalid_argument if the delta exceeds the total.
 */
svanes::Timeline ReadTimeline(svanes::MessageReader& reader)
{
    const auto total = reader.ReadUint64();
    const auto delta = reader.ReadUint64();
    if (delta > total) {
        throw std::invalid_argument("Encoded timeline has a delta larger than its total.");
    }
    svanes::Timeline timeline;
    timeline.Advance(total - delta);
    timeline.Advance(delta);
    return timeline;
}

}

void GooseSimulation::Initialize(svanes::GameContext& context)
{
    if (initialized) {
        throw std::logic_error("GooseSimulation has already been initialized.");
    }
    context.automatic_simulation = false;
    textures = Goose::LoadTextures(context.assets);
    enemy.Spawn(context.world, {1600.0F, 300.0F}, 120.0F, 10.0F);
    departed_owner = context.world.CreateEntity();
    initialized = true;
}

void GooseSimulation::AddPlayer(svanes::Registry& world, svanes::PeerId peer)
{
    if (!initialized) {
        throw std::logic_error("GooseSimulation::AddPlayer called before Initialize.");
    }
    if (peer.value == 0 || (!players.empty() && players.back().peer.value >= peer.value)) {
        throw std::invalid_argument("GooseSimulation::AddPlayer requires a nonzero id above every current player.");
    }
    auto spawn_x = kSpawnX[peer.value % kSpawnX.size()];
    for (const auto x : kSpawnX) {
        const bool occupied = std::any_of(players.begin(), players.end(), [&](const auto& player) {
            const auto& transform = world.GetComponent<svanes::Transform>(player.goose.GetEntity());
            return std::hypot(transform.x - x, transform.y - kSpawnY) < kSpawnClearance;
        });
        if (!occupied) {
            spawn_x = x;
            break;
        }
    }
    players.push_back({peer, {}});
    players.back().goose.Spawn(world, textures, spawn_x, kSpawnY);
}

void GooseSimulation::Step(svanes::Registry& world, svanes::Vector2D gravity,
                            std::span<const GooseIntent> inputs)
{
    if (!initialized) {
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
    for (const auto& hit : UpdateBullets(world, bullet_bounds, BulletTargets(world))) {
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
    if (!initialized) {
        throw std::logic_error("GooseSimulation::Capture called before Initialize.");
    }
    GooseWorldSnapshot snapshot{tick, {}, enemy.Capture(world), CaptureBullets(world), next_bullet_id};
    snapshot.geese.reserve(players.size());
    for (const auto& player : players) {
        snapshot.geese.push_back(player.goose.Capture(world));
    }
    return snapshot;
}

void GooseSimulation::Encode(svanes::MessageWriter& writer, const GooseWorldSnapshot& snapshot)
{
    writer.WriteUint64(snapshot.tick);
    writer.WriteUint64(snapshot.next_bullet_id);
    writer.WriteUint32(static_cast<std::uint32_t>(snapshot.geese.size()));
    for (const auto& goose : snapshot.geese) {
        WriteTransform(writer, goose.transform);
        WriteMotion(writer, goose.motion);
        WriteTimeline(writer, goose.timeline);
        writer.WriteBool(goose.sprite.source.has_value());
        if (goose.sprite.source) {
            writer.WriteFloat32(goose.sprite.source->x);
            writer.WriteFloat32(goose.sprite.source->y);
            writer.WriteFloat32(goose.sprite.source->width);
            writer.WriteFloat32(goose.sprite.source->height);
        }
        writer.WriteBool(goose.animation.has_value());
        if (goose.animation) {
            writer.WriteInt32(goose.animation->frame_width);
            writer.WriteInt32(goose.animation->frame_height);
            writer.WriteInt32(goose.animation->frame_count);
            writer.WriteInt32(goose.animation->current_frame);
            writer.WriteUint64(goose.animation->tics_per_frame);
            writer.WriteUint64(goose.animation->elapsed_tics);
        }
        writer.WriteUint8(static_cast<std::uint8_t>(goose.state));
        writer.WriteBool(goose.grounded);
        writer.WriteUint64(goose.fly_time_remaining);
        writer.WriteUint64(goose.dash_timer);
        writer.WriteUint64(goose.dash_cooldown);
        writer.WriteUint64(goose.fire_cooldown);
        writer.WriteUint64(goose.knockback_timer);
        writer.WriteUint64(goose.invincible_timer);
    }
    WriteTransform(writer, snapshot.enemy.transform);
    writer.WriteFloat32(snapshot.enemy.health.current);
    writer.WriteFloat32(snapshot.enemy.health.max);
    writer.WriteUint64(snapshot.enemy.fire_cooldown);
    writer.WriteBool(snapshot.enemy.alive);
    writer.WriteUint32(static_cast<std::uint32_t>(snapshot.bullets.size()));
    for (const auto& shot : snapshot.bullets) {
        writer.WriteUint64(shot.bullet.id);
        writer.WriteUint32(shot.bullet.owner_key);
        WriteTransform(writer, shot.transform);
        WriteMotion(writer, shot.motion);
        WriteTimeline(writer, shot.timeline);
        writer.WriteUint8(shot.shape.color.red);
        writer.WriteUint8(shot.shape.color.green);
        writer.WriteUint8(shot.shape.color.blue);
        writer.WriteUint8(shot.shape.color.alpha);
    }
}

GooseWorldSnapshot GooseSimulation::Decode(svanes::MessageReader& reader) const
{
    GooseWorldSnapshot snapshot{};
    snapshot.tick = reader.ReadUint64();
    snapshot.next_bullet_id = reader.ReadUint64();
    if (reader.ReadUint32() != players.size()) {
        throw std::invalid_argument("Encoded world has a different player count.");
    }
    for (std::size_t index = 0; index < players.size(); ++index) {
        GooseSnapshot goose{};
        goose.transform = ReadTransform(reader);
        goose.motion = ReadMotion(reader);
        goose.timeline = ReadTimeline(reader);
        if (reader.ReadBool()) {
            goose.sprite.source = svanes::Rectangle2D{reader.ReadFloat32(), reader.ReadFloat32(),
                reader.ReadFloat32(), reader.ReadFloat32()};
        }
        if (reader.ReadBool()) {
            svanes::SpriteAnimation animation;
            animation.frame_width = reader.ReadInt32();
            animation.frame_height = reader.ReadInt32();
            animation.frame_count = reader.ReadInt32();
            animation.current_frame = reader.ReadInt32();
            animation.tics_per_frame = reader.ReadUint64();
            animation.elapsed_tics = reader.ReadUint64();
            goose.animation = animation;
        }
        const auto state = reader.ReadUint8();
        if (state > static_cast<std::uint8_t>(GooseState::Flying)) {
            throw std::invalid_argument("Encoded goose has an unknown state.");
        }
        goose.state = static_cast<GooseState>(state);
        goose.grounded = reader.ReadBool();
        goose.fly_time_remaining = reader.ReadUint64();
        goose.dash_timer = reader.ReadUint64();
        goose.dash_cooldown = reader.ReadUint64();
        goose.fire_cooldown = reader.ReadUint64();
        goose.knockback_timer = reader.ReadUint64();
        goose.invincible_timer = reader.ReadUint64();
        snapshot.geese.push_back(goose);
    }
    snapshot.enemy.transform = ReadTransform(reader);
    snapshot.enemy.health.current = reader.ReadFloat32();
    snapshot.enemy.health.max = reader.ReadFloat32();
    snapshot.enemy.fire_cooldown = reader.ReadUint64();
    snapshot.enemy.alive = reader.ReadBool();
    const auto bullet_count = reader.ReadUint32();
    const svanes::Rectangle2D body{.width = BulletSize, .height = BulletSize};
    for (std::uint32_t index = 0; index < bullet_count; ++index) {
        BulletSnapshot shot{};
        shot.bullet.id = reader.ReadUint64();
        shot.bullet.owner_key = reader.ReadUint32();
        shot.transform = ReadTransform(reader);
        shot.motion = ReadMotion(reader);
        shot.timeline = ReadTimeline(reader);
        shot.shape.color = {reader.ReadUint8(), reader.ReadUint8(), reader.ReadUint8(), reader.ReadUint8()};
        shot.shape.geometry = body;
        shot.collider.geometry = body;
        shot.bullet.owner = departed_owner;
        if (shot.bullet.owner_key == 0) {
            shot.bullet.owner = enemy.GetEntity();
        }
        for (const auto& player : players) {
            if (player.peer.value == shot.bullet.owner_key) {
                shot.bullet.owner = player.goose.GetEntity();
            }
        }
        snapshot.bullets.push_back(shot);
    }
    return snapshot;
}

std::uint64_t GooseSimulation::Hash(const GooseWorldSnapshot& snapshot)
{
    svanes::MessageWriter writer;
    Encode(writer, snapshot);
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
    if (!initialized) {
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

std::vector<svanes::PeerId> GooseSimulation::Roster() const
{
    std::vector<svanes::PeerId> roster;
    for (const auto& player : players) {
        roster.push_back(player.peer);
    }
    return roster;
}

std::uint64_t GooseSimulation::Tick() const
{
    return tick;
}

void GooseSimulation::RemovePlayer(svanes::Registry& world, svanes::PeerId peer)
{
    const auto found = std::find_if(players.begin(), players.end(),
        [&](const auto& player) { return player.peer == peer; });
    if (found == players.end()) {
        throw std::invalid_argument("GooseSimulation::RemovePlayer: unknown peer.");
    }
    world.DestroyEntity(found->goose.GetEntity());
    players.erase(found);
}

std::vector<svanes::Entity> GooseSimulation::BulletTargets(const svanes::Registry& world) const
{
    std::vector<svanes::Entity> targets;
    world.ForEach<svanes::Transform, svanes::Collider2D, Solid>(
        [&](svanes::Entity entity, const svanes::Transform&, const svanes::Collider2D&, const Solid&) {
            targets.push_back(entity);
        });
    std::sort(targets.begin(), targets.end());
    for (const auto& player : players) {
        targets.push_back(player.goose.GetEntity());
    }
    if (enemy.IsAlive()) {
        targets.push_back(enemy.GetEntity());
    }
    return targets;
}
