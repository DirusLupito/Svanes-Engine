#include "goose_simulation.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/deterministic_math.hpp>
#include <svanes/game.hpp>
#include <svanes/input.hpp>
#include <svanes/network/component_serialization.hpp>
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
constexpr std::uint32_t kDoubleTapTicks = 25;
constexpr float kMaximumAim = 100000.0F;

}

GooseSimulation::GooseSimulation(svanes::GameContext& context)
    : world(context.world), gravity(context.gravity)
{
    context.automatic_simulation = false;
    textures = Goose::LoadTextures(context.assets);
    enemy.Spawn(world, {1600.0F, 300.0F}, 120.0F, 10.0F, NextStableId());
    departed_owner = world.CreateEntity();
}

void GooseSimulation::AddPlayer(svanes::PeerId peer)
{
    if (peer.value == 0 || (!players.empty() && players.back().peer.value >= peer.value)) {
        throw std::invalid_argument("GooseSimulation::AddPlayer requires a nonzero id above every current player.");
    }
    auto spawn_x = kSpawnX[peer.value % kSpawnX.size()];
    for (const auto x : kSpawnX) {
        const bool occupied = std::any_of(players.begin(), players.end(), [&](const auto& player) {
            const auto& transform = world.GetComponent<svanes::Transform>(player.goose.GetEntity());
            return svanes::Length(transform.x - x, transform.y - kSpawnY) < kSpawnClearance;
        });
        if (!occupied) {
            spawn_x = x;
            break;
        }
    }
    players.push_back({peer, {}});
    players.back().goose.Spawn(world, textures, spawn_x, kSpawnY, NextStableId());
}

std::vector<GooseIntentUse> GooseSimulation::Step(std::span<const GooseIntent> inputs)
{
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
            !std::isfinite(input.aim.x) || !std::isfinite(input.aim.y) ||
            std::abs(input.aim.x) > kMaximumAim || std::abs(input.aim.y) > kMaximumAim) {
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
    std::vector<GooseIntentUse> use;
    use.reserve(players.size());
    for (std::size_t index = 0; index < players.size(); ++index) {
        use.push_back(players[index].goose.Advance(world, inputs[index], steps[index].delta_tics));
    }
    const double elapsed_seconds = static_cast<double>(tick + 1) *
        static_cast<double>(GooseStepTics) / static_cast<double>(svanes::TicsPerSecond);
    EnemyIntent enemy_intent{};
    enemy_intent.move_to = {
        1600.0F + svanes::Sin(static_cast<float>(elapsed_seconds * 0.8)) * 700.0F,
        300.0F
    };
    if (enemy.IsAlive()) {
        const auto position = enemy.Position(world);
        enemy_intent.fire = true;
        enemy_intent.aim_point = {position.x, position.y + 1000.0F};
        enemy.Advance(world, enemy_intent, GooseStepTics);
    }
    std::vector<svanes::Entity> new_bullets;
    world.ForEach<Bullet>([&](svanes::Entity entity, const Bullet&) {
        if (!world.HasComponent<svanes::StableId>(entity)) {
            new_bullets.push_back(entity);
        }
    });
    std::sort(new_bullets.begin(), new_bullets.end());
    for (const auto entity : new_bullets) {
        world.AddComponent<svanes::StableId>(entity, NextStableId());
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
    return use;
}

GooseWorldSnapshot GooseSimulation::Capture() const
{
    GooseWorldSnapshot snapshot{tick, {}, enemy.Capture(world), CaptureBullets(world), next_stable_id};
    snapshot.geese.reserve(players.size());
    for (const auto& player : players) {
        snapshot.geese.push_back(player.goose.Capture(world));
    }
    return snapshot;
}

void GooseSimulation::Encode(svanes::MessageWriter& writer, const GooseWorldSnapshot& snapshot)
{
    writer.WriteUint64(snapshot.tick);
    writer.WriteUint64(snapshot.next_stable_id);
    writer.WriteUint32(static_cast<std::uint32_t>(snapshot.geese.size()));
    for (const auto& goose : snapshot.geese) {
        writer.WriteUint64(goose.id.value);
        svanes::WriteTransform(writer, goose.transform);
        svanes::WriteKinematic(writer, goose.motion);
        svanes::WriteTimeline(writer, goose.timeline);
        writer.WriteBool(goose.sprite.source.has_value());
        if (goose.sprite.source) {
            svanes::WriteRectangle(writer, *goose.sprite.source);
        }
        writer.WriteBool(goose.animation.has_value());
        if (goose.animation) {
            svanes::WriteSpriteAnimation(writer, *goose.animation);
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
    svanes::WriteTransform(writer, snapshot.enemy.transform);
    writer.WriteFloat32(snapshot.enemy.health.current);
    writer.WriteFloat32(snapshot.enemy.health.max);
    writer.WriteUint64(snapshot.enemy.fire_cooldown);
    writer.WriteBool(snapshot.enemy.alive);
    writer.WriteUint32(static_cast<std::uint32_t>(snapshot.bullets.size()));
    for (const auto& shot : snapshot.bullets) {
        writer.WriteUint64(shot.id.value);
        writer.WriteUint64(shot.bullet.owner_id);
        svanes::WriteTransform(writer, shot.transform);
        svanes::WriteKinematic(writer, shot.motion);
        svanes::WriteTimeline(writer, shot.timeline);
        svanes::WriteColor(writer, shot.shape.color);
    }
}

GooseWorldSnapshot GooseSimulation::Decode(svanes::MessageReader& reader) const
{
    GooseWorldSnapshot snapshot{};
    snapshot.tick = reader.ReadUint64();
    snapshot.next_stable_id = reader.ReadUint64();
    if (reader.ReadUint32() != players.size()) {
        throw std::invalid_argument("Encoded world has a different player count.");
    }
    for (std::size_t index = 0; index < players.size(); ++index) {
        GooseSnapshot goose{};
        goose.id.value = reader.ReadUint64();
        goose.transform = svanes::ReadTransform(reader);
        goose.motion = svanes::ReadKinematic(reader);
        goose.timeline = svanes::ReadTimeline(reader);
        if (reader.ReadBool()) {
            goose.sprite.source = svanes::ReadRectangle(reader);
        }
        if (reader.ReadBool()) {
            goose.animation = svanes::ReadSpriteAnimation(reader);
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
    snapshot.enemy.transform = svanes::ReadTransform(reader);
    snapshot.enemy.health.current = reader.ReadFloat32();
    snapshot.enemy.health.max = reader.ReadFloat32();
    snapshot.enemy.fire_cooldown = reader.ReadUint64();
    snapshot.enemy.alive = reader.ReadBool();
    const auto bullet_count = reader.ReadUint32();
    const svanes::Rectangle2D body{.width = BulletSize, .height = BulletSize};
    for (std::uint32_t index = 0; index < bullet_count; ++index) {
        BulletSnapshot shot{};
        shot.id.value = reader.ReadUint64();
        shot.bullet.owner_id = reader.ReadUint64();
        shot.transform = svanes::ReadTransform(reader);
        shot.motion = svanes::ReadKinematic(reader);
        shot.timeline = svanes::ReadTimeline(reader);
        shot.shape.color = svanes::ReadColor(reader);
        shot.shape.geometry = body;
        shot.collider.geometry = body;
        snapshot.bullets.push_back(shot);
    }
    return snapshot;
}

void GooseSimulation::Restore(const GooseWorldSnapshot& snapshot)
{
    if (snapshot.geese.size() != players.size()) {
        throw std::invalid_argument("GooseSimulation snapshot does not match the player count.");
    }
    for (std::size_t index = 0; index < players.size(); ++index) {
        players[index].goose.Restore(world, snapshot.geese[index]);
    }
    enemy.Restore(world, snapshot.enemy);
    RestoreBullets(world, snapshot.bullets, departed_owner);
    next_stable_id = snapshot.next_stable_id;
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

void GooseSimulation::RemovePlayer(svanes::PeerId peer)
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

svanes::StableId GooseSimulation::NextStableId()
{
    if (next_stable_id == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("GooseSimulation exhausted its stable ids.");
    }
    return {next_stable_id++};
}

svanes::NetworkMessage GooseSimulation::Save() const
{
    svanes::MessageWriter writer;
    Encode(writer, Capture());
    return writer.Finish();
}

void GooseSimulation::Load(std::span<const std::byte> saved)
{
    svanes::MessageReader reader(saved);
    const auto snapshot = Decode(reader);
    if (reader.Remaining() != 0) {
        throw std::invalid_argument("Saved goose world has trailing data.");
    }
    Restore(snapshot);
}

void GooseSimulation::CaptureInput(const svanes::FrameContext& frame, svanes::PeerId local)
{
    controls.move = (frame.input.IsDown(svanes::Key::D) ? 1.0F : 0.0F)
        - (frame.input.IsDown(svanes::Key::A) ? 1.0F : 0.0F);
    controls.jump = frame.input.IsDown(svanes::Key::Space);
    controls.fire = frame.input.IsMouseButtonDown(svanes::MouseButton::Left);
    controls.aim = {};
    if (controls.fire) {
        const auto mouse = frame.input.MousePosition();
        const auto aim = frame.camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});
        const auto& goose = world.GetComponent<svanes::Transform>(PlayerEntity(local));
        controls.aim = {std::clamp(aim.x - goose.x, -kMaximumAim, kMaximumAim),
            std::clamp(aim.y - goose.y, -kMaximumAim, kMaximumAim)};
    }
    controls.left_pressed |= frame.input.WasPressed(svanes::Key::A);
    controls.right_pressed |= frame.input.WasPressed(svanes::Key::D);
}

GooseIntent GooseSimulation::TakeInput()
{
    if (controls.left_tap_ticks > 0) {
        --controls.left_tap_ticks;
    }
    if (controls.right_tap_ticks > 0) {
        --controls.right_tap_ticks;
    }
    GooseIntent input{};
    input.move.x = controls.move;
    input.jump = controls.jump;
    input.fire = controls.fire;
    input.aim = controls.aim;
    if (controls.left_pressed) {
        if (controls.left_tap_ticks > 0) {
            input.dash = -1.0F;
            controls.left_tap_ticks = 0;
        } else {
            controls.left_tap_ticks = kDoubleTapTicks;
        }
    }
    if (controls.right_pressed) {
        if (controls.right_tap_ticks > 0) {
            input.dash = 1.0F;
            controls.right_tap_ticks = 0;
        } else {
            controls.right_tap_ticks = kDoubleTapTicks;
        }
    }
    controls.left_pressed = false;
    controls.right_pressed = false;
    return input;
}

void GooseSimulation::EncodeInput(svanes::MessageWriter& writer, const GooseIntent& input) const
{
    writer.WriteUint8(static_cast<std::uint8_t>(input.move.x + 1.0F));
    writer.WriteUint8(static_cast<std::uint8_t>(input.dash + 1.0F));
    writer.WriteBool(input.jump);
    writer.WriteBool(input.fire);
    writer.WriteFloat32(input.aim.x);
    writer.WriteFloat32(input.aim.y);
}

GooseIntent GooseSimulation::DecodeInput(svanes::MessageReader& reader) const
{
    const auto move = reader.ReadUint8();
    const auto dash = reader.ReadUint8();
    GooseIntent input{};
    input.jump = reader.ReadBool();
    input.fire = reader.ReadBool();
    input.aim = {reader.ReadFloat32(), reader.ReadFloat32()};
    if (move > 2 || dash > 2 || !std::isfinite(input.aim.x) || !std::isfinite(input.aim.y) ||
        std::abs(input.aim.x) > kMaximumAim || std::abs(input.aim.y) > kMaximumAim) {
        throw std::invalid_argument("Goose input has an invalid direction or aim.");
    }
    input.move.x = static_cast<float>(move) - 1.0F;
    input.dash = static_cast<float>(dash) - 1.0F;
    return input;
}

GooseIntent GooseSimulation::Predict(const GooseIntent& last) const
{
    auto input = last;
    input.dash = 0.0F;
    return input;
}

bool GooseSimulation::ChangesStep(const GooseIntent& used, const GooseIntent& actual,
                                  const GooseIntentUse& use) const
{
    return ::ChangesStep(used, actual, use);
}
