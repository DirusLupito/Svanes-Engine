#include "p2p_game.hpp"

#include "tilemap_parser.hpp"

#include <svanes/input.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/registry.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <svanes/tilemaps/tilemap.hpp>
#include <svanes/utility/hash.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
constexpr svanes::SessionId kSession = 0xD0B1E71EULL;
constexpr std::uint16_t kHostPort = 5560;
constexpr std::uint16_t kJoinPort = 5561;
constexpr svanes::TicCount kStepTics = 10000;
constexpr float kGravity = 1500.0F * kChrisWorldScale;
constexpr float kSpeed = 360.0F * kChrisWorldScale;
constexpr float kJump = 800.0F * kChrisWorldScale;
constexpr svanes::Color kP2PPlatformColor{160, 82, 45, 255};
constexpr svanes::Color kP2PBackgroundColor{17, 24, 39, 255};
constexpr std::int32_t kIdleFrameCount = 8;
constexpr std::int32_t kRunningFrameCount = 10;
constexpr std::int32_t kCharacterFrameWidth = 288;
constexpr std::int32_t kCharacterFrameHeight = 320;
constexpr float kCharacterFramesPerSecond = 12.0F;
constexpr float kCharacterSpawnLeft = 20.0F;
constexpr float kCharacterColliderLeftInset = 10.0F * 0.75F;
constexpr float kCharacterColliderRightInset = 11.0F * 0.75F;
constexpr float kCharacterColliderWidth = kChrisCharacterWidth -
                                          kCharacterColliderLeftInset -
                                          kCharacterColliderRightInset;
constexpr float kCharacterColliderOffsetX =
    (kCharacterColliderLeftInset - kCharacterColliderRightInset) * 0.5F;
constexpr float kPlatformSlideDistance = 360.0F * kChrisWorldScale;
constexpr float kPlatformSlideSpeed = 1400.0F * kChrisWorldScale;
constexpr float kPlatformBeatSeconds = 0.5F;
const svanes::TicCount kPlatformHoldTics =
    svanes::SecondsToTics(kPlatformBeatSeconds * 2.0F);
constexpr float kPlatformY =
    kChrisWorldHeight - kChrisGroundHeight -
    kChrisPlatformTopHeightAboveGround + kChrisPlatformHeight * 0.5F;

void ResolveCharacterAxis(svanes::Registry &world, svanes::Entity character,
                          svanes::Entity tilemap, bool horizontal) {
    auto &transform = world.GetComponent<svanes::Transform>(character);
    const auto &collider = world.GetComponent<svanes::Collider2D>(character);
    for (int iteration = 0; iteration < 4; ++iteration) {
        std::optional<svanes::Collision2D> deepest;
        const auto consider = [&](const svanes::Collision2D &collision) {
            const bool axis_aligned = horizontal
                ? collision.normal.y == 0.0F
                : collision.normal.x == 0.0F;
            if (!axis_aligned || (deepest &&
                collision.penetration_depth <= deepest->penetration_depth))
                return;
            deepest = collision;
        };
        world.ForEach<svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity other, const svanes::Transform &other_transform,
                const svanes::Collider2D &other_collider) {
                if (other == character) return;
                for (const auto &collision : svanes::DetectCollisions(
                         collider.geometry, transform, other_collider.geometry,
                         other_transform))
                    consider(collision);
            });
        const auto &map_transform = world.GetComponent<svanes::Transform>(tilemap);
        const auto &map = world.GetComponent<svanes::TileMap>(tilemap);
        for (const auto &collision : svanes::DetectTileMapCollisions(
                 collider.geometry, transform, map, map_transform))
            consider(collision);
        if (!deepest) break;
        if (horizontal) transform.x += deepest->normal.x * deepest->penetration_depth;
        else transform.y += deepest->normal.y * deepest->penetration_depth;
    }
}

std::uint64_t RulesHash(const svanes::SyncSettings &settings) {
    svanes::MessageWriter writer;
    writer.WriteUint64(settings.step_tics);
    writer.WriteFloat32(kGravity);
    writer.WriteFloat32(kSpeed);
    writer.WriteFloat32(kJump);
    writer.WriteUint64(settings.prediction_ticks);
    writer.WriteUint64(settings.history_ticks);
    return svanes::HashBytes(writer.Finish().bytes);
}
}

DoubleTimeP2PSimulation::DoubleTimeP2PSimulation(
    svanes::GameContext &context, svanes::TextureHandle texture,
    svanes::TextureHandle run_texture, svanes::Entity tilemap_entity)
    : world(context.world), idle_texture(texture), running_texture(run_texture),
      tilemap(tilemap_entity) {
    context.automatic_simulation = false;
    platform = world.CreateEntity();
    world.AddComponent<svanes::Transform>(platform, svanes::Transform{platform_x, kPlatformY, 0.0F});
    world.AddComponent<svanes::SolidShape>(
        platform, svanes::SolidShape{kP2PPlatformColor, svanes::Rectangle2D{0.0F, 0.0F, kChrisPlatformWidth,
                                    kChrisPlatformHeight}});
    world.AddComponent<svanes::Collider2D>(
        platform, svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F,
                                                        kChrisPlatformWidth,
                                                        kChrisPlatformHeight}});
    world.AddComponent<svanes::ZOrder>(platform, svanes::ZOrder{1});
}

void DoubleTimeP2PSimulation::AddPlayer(svanes::PeerId peer) {
    if (peer.value == 0 || (!players.empty() && players.back().peer.value >= peer.value))
        throw std::invalid_argument("Double Time P2P player order is invalid.");
    const float x = kCharacterSpawnLeft;
    const auto entity = peer.value == 1 ? world.CreateEntity() : svanes::Entity{};
    if (peer.value == 1) {
        world.AddComponent<svanes::Transform>(entity, svanes::Transform{x, 80.0F, 0.0F});
        world.AddComponent<svanes::Sprite>(entity, svanes::Sprite{
            .texture = idle_texture,
            .geometry = svanes::Rectangle2D{0.0F, 0.0F, kChrisCharacterWidth, kChrisCharacterHeight}});
        world.AddComponent<svanes::Collider2D>(
            entity, svanes::Collider2D{svanes::Rectangle2D{
                kCharacterColliderOffsetX, 0.0F, kCharacterColliderWidth,
                kChrisCharacterHeight}});
        world.AddComponent<svanes::Timeline>(entity);
        world.AddComponent<svanes::SpriteAnimation>(
            entity, svanes::SpriteAnimation{
                .frame_width = kCharacterFrameWidth,
                .frame_height = kCharacterFrameHeight,
                .frame_count = kIdleFrameCount,
                .tics_per_frame = svanes::SecondsToTics(
                    1.0 / kCharacterFramesPerSecond),
            });
        world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{2});
    }
    players.push_back({peer, entity, x, 80.0F});
}

void DoubleTimeP2PSimulation::RemovePlayer(svanes::PeerId peer) {
    const auto found = std::find_if(players.begin(), players.end(),
        [peer](const Player &player) { return player.peer == peer; });
    if (found == players.end()) throw std::invalid_argument("Unknown P2P player.");
    players.erase(found);
}

std::vector<DoubleTimeP2PInputUse> DoubleTimeP2PSimulation::Step(
    std::span<const DoubleTimeP2PInput> inputs) {
    if (inputs.size() != players.size()) throw std::invalid_argument("P2P input count mismatch.");
    const float dt = static_cast<float>(kStepTics) / static_cast<float>(svanes::TicsPerSecond);
    const float platform_start_x = platform_x;
    switch (platform_state) {
    case PlatformState::Idle:
        if (inputs.size() > 1 && inputs[1].jump) {
            platform_state = PlatformState::SlidingLeft;
        }
        break;
    case PlatformState::SlidingLeft:
        platform_x -= kPlatformSlideSpeed * dt;
        if (platform_x <= kChrisPlatformLeft + kChrisPlatformWidth * 0.5F -
                              kPlatformSlideDistance) {
            platform_x = kChrisPlatformLeft + kChrisPlatformWidth * 0.5F -
                          kPlatformSlideDistance;
            platform_state = PlatformState::Holding;
            platform_holding_tics = kPlatformHoldTics;
        }
        break;
    case PlatformState::Holding:
        platform_holding_tics =
            platform_holding_tics > kStepTics
                ? platform_holding_tics - kStepTics
                : 0;
        if (platform_holding_tics == 0) platform_state = PlatformState::SlidingBack;
        break;
    case PlatformState::SlidingBack:
        platform_x += kPlatformSlideSpeed * dt;
        if (platform_x >= kChrisPlatformLeft + kChrisPlatformWidth * 0.5F) {
            platform_x = kChrisPlatformLeft + kChrisPlatformWidth * 0.5F;
            platform_state = PlatformState::Idle;
        }
        break;
    }
    const float platform_delta_x = platform_x - platform_start_x;
    world.GetComponent<svanes::Transform>(platform).x = platform_x;
    if (!players.empty()) {
        auto &player = players.front();
        const float previous_x = player.x;
        if (player.grounded_on_platform) player.x += platform_delta_x;
        player.horizontal_input = inputs.front().horizontal;
        player.x += inputs.front().horizontal * kSpeed * dt;
        player.velocity_y += kGravity * dt;
        if (inputs.front().jump && player.grounded)
            player.velocity_y = -kJump;
        player.y += player.velocity_y * dt;
        player.grounded_on_platform = false;
        if (player.entity != svanes::Entity{}) {
            // Collision queries operate on the ECS transform. Copy the
            // predicted simulation position into it before resolving, or the
            // resolver will overwrite movement with the previous position.
            world.GetComponent<svanes::Transform>(player.entity) =
                {player.x, player.y, 0.0F};
            const float pre_collision_y = player.y;
            ResolveCharacterAxis(world, player.entity, tilemap, true);
            ResolveCharacterAxis(world, player.entity, tilemap, false);
            const auto &character_transform =
                world.GetComponent<svanes::Transform>(player.entity);
            const bool landed = player.velocity_y >= 0.0F &&
                                character_transform.y < pre_collision_y;
            if (landed) player.velocity_y = 0.0F;
            player.grounded = landed;
            const float character_left = player.x + kCharacterColliderOffsetX -
                                         kCharacterColliderWidth * 0.5F;
            const float character_right = character_left + kCharacterColliderWidth;
            const float platform_left = platform_x - kChrisPlatformWidth * 0.5F;
            const float platform_right = platform_x + kChrisPlatformWidth * 0.5F;
            const float platform_top = kPlatformY - kChrisPlatformHeight * 0.5F;
            player.grounded_on_platform = landed && character_right > platform_left &&
                character_left < platform_right &&
                std::fabs(character_transform.y + kChrisCharacterHeight * 0.5F -
                          platform_top) < 0.1F;
            player.x = character_transform.x;
            player.y = character_transform.y;
            world.GetComponent<svanes::Transform>(player.entity) =
                {player.x, player.y, 0.0F};
        }
        // Match the client-server animation rule: holding a direction alone
        // is not enough. The resolved simulation position must have moved.
        presentation_running =
            std::fabs(player.horizontal_input) > 0.01F &&
            std::fabs(player.x - previous_x) > 0.5F;
    }
    return std::vector<DoubleTimeP2PInputUse>(players.size());
}

void DoubleTimeP2PSimulation::UpdatePresentation(svanes::TicCount real_delta_tics) {
    if (players.empty() || players.front().entity == svanes::Entity{}) return;
    auto &player = players.front();
    const bool running = presentation_running;

    auto &sprite = world.GetComponent<svanes::Sprite>(player.entity);
    auto &animation = world.GetComponent<svanes::SpriteAnimation>(player.entity);
    const auto desired_texture = running ? running_texture : idle_texture;
    const auto desired_frame_count = running ? kRunningFrameCount : kIdleFrameCount;
    if (!presentation_state_initialized ||
        sprite.texture.id != desired_texture.id ||
        animation.frame_count != desired_frame_count) {
        sprite.texture = desired_texture;
        animation.frame_count = desired_frame_count;
        animation.current_frame = 0;
        animation.elapsed_tics = 0;
        presentation_state_initialized = true;
    }
    // Presentation time is intentionally separate from the rollbackable
    // simulation timeline. Replaying a simulation tick must not replay or
    // discard visible animation time.
    animation.elapsed_tics += real_delta_tics;
    if (animation.tics_per_frame > 0 &&
        animation.elapsed_tics >= animation.tics_per_frame) {
        const auto frames = animation.elapsed_tics / animation.tics_per_frame;
        animation.elapsed_tics %= animation.tics_per_frame;
        animation.current_frame = static_cast<std::int32_t>(
            (animation.current_frame + frames) % animation.frame_count);
    }
    sprite.source = svanes::Rectangle2D{
        static_cast<float>(animation.current_frame * animation.frame_width) +
            animation.frame_width * 0.5F,
        animation.frame_height * 0.5F,
        static_cast<float>(animation.frame_width),
        static_cast<float>(animation.frame_height)};
}

void DoubleTimeP2PSimulation::CaptureInput(const svanes::FrameContext &frame, svanes::PeerId local) {
    local_peer = local;
    local_input = {};
    if (local.value == 1) {
        local_input.horizontal = (frame.input.IsDown(svanes::Key::Right) ? 1.0F : 0.0F) -
                                 (frame.input.IsDown(svanes::Key::Left) ? 1.0F : 0.0F);
        local_input.jump = frame.input.WasPressed(svanes::Key::Space);
    } else if (local.value == 2) {
        local_input.jump = frame.input.WasPressed(svanes::Key::Space);
    }
}
DoubleTimeP2PInput DoubleTimeP2PSimulation::TakeInput() { return std::exchange(local_input, {}); }
void DoubleTimeP2PSimulation::EncodeInput(svanes::MessageWriter &writer, const DoubleTimeP2PInput &input) const {
    writer.WriteFloat32(input.horizontal); writer.WriteBool(input.jump);
}
DoubleTimeP2PInput DoubleTimeP2PSimulation::DecodeInput(svanes::MessageReader &reader) const {
    DoubleTimeP2PInput input{reader.ReadFloat32(), reader.ReadBool()};
    if (input.horizontal < -1.0F || input.horizontal > 1.0F) throw std::invalid_argument("Invalid P2P input.");
    return input;
}

svanes::NetworkMessage DoubleTimeP2PSimulation::Save() const {
    svanes::MessageWriter writer;
    writer.WriteFloat32(platform_x);
    writer.WriteUint8(static_cast<std::uint8_t>(platform_state));
    writer.WriteUint64(platform_holding_tics);
    writer.WriteUint32(static_cast<std::uint32_t>(players.size()));
    for (const auto &player : players) {
        writer.WriteUint32(player.peer.value); writer.WriteFloat32(player.x);
        writer.WriteFloat32(player.y); writer.WriteFloat32(player.horizontal_input);
        writer.WriteFloat32(player.velocity_y);
        writer.WriteBool(player.grounded);
        writer.WriteBool(player.grounded_on_platform);
    }
    return writer.Finish();
}
void DoubleTimeP2PSimulation::Load(std::span<const std::byte> saved) {
    svanes::NetworkMessage message; message.bytes.assign(saved.begin(), saved.end());
    svanes::MessageReader reader(message);
    platform_x = reader.ReadFloat32();
    const auto state = reader.ReadUint8();
    if (state > static_cast<std::uint8_t>(PlatformState::SlidingBack))
        throw std::invalid_argument("Invalid P2P platform state.");
    platform_state = static_cast<PlatformState>(state);
    platform_holding_tics = reader.ReadUint64();
    if (reader.ReadUint32() != players.size()) throw std::invalid_argument("P2P snapshot player count mismatch.");
    for (auto &player : players) {
        if (reader.ReadUint32() != player.peer.value) throw std::invalid_argument("P2P snapshot roster mismatch.");
        player.x = reader.ReadFloat32(); player.y = reader.ReadFloat32();
        player.horizontal_input = reader.ReadFloat32();
        player.velocity_y = reader.ReadFloat32();
        player.grounded = reader.ReadBool();
        player.grounded_on_platform = reader.ReadBool();
        if (player.entity != svanes::Entity{})
            world.GetComponent<svanes::Transform>(player.entity) =
                {player.x, player.y, 0.0F};
    }
    world.GetComponent<svanes::Transform>(platform).x = platform_x;
    if (reader.Remaining() != 0) throw std::invalid_argument("Trailing P2P snapshot data.");
}
svanes::Entity DoubleTimeP2PSimulation::PlayerEntity(svanes::PeerId peer) const {
    for (const auto &player : players) if (player.peer == peer) return player.entity;
    throw std::invalid_argument("Unknown P2P player.");
}

DoubleTimeP2PGame::DoubleTimeP2PGame(std::uint16_t local_port,
                                     std::optional<svanes::UdpAddress> join)
    : port(local_port), entry(std::move(join)) {}

void DoubleTimeP2PGame::Initialize(svanes::GameContext &context) {
    context.camera.scale_mode = svanes::ScaleMode::Proportional;
    context.camera.zoom = context.camera.Viewport().height /
                          context.camera.Scale() / kChrisWorldHeight;
    const auto atlas = context.assets.LoadTexture(std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/TestTileSet.png");
    const auto idle = context.assets.LoadTexture(std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/sheet_umeko-idle.png");
    const auto running = context.assets.LoadTexture(std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/sheet_umeko-run.png");
    const auto ground = context.world.CreateEntity();
    auto tilemap = CreateDoubleTimeTileMapFromPng(
        std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" + kChrisTileMapFilename,
        atlas);
    const auto tilemap_transform = GetCenteredTileMapTransform(
        tilemap, kChrisWorldWidth * 0.5F, kChrisWorldHeight * 0.5F);
    context.world.AddComponent<svanes::Transform>(ground, tilemap_transform);
    context.world.AddComponent<svanes::TileMap>(ground, std::move(tilemap));
    const auto &map = context.world.GetComponent<svanes::TileMap>(ground);
    // The proportional camera can show more world space on wider windows.
    // Give the background generous coverage instead of tying it to the
    // tilemap's transparent bounds.
    const float map_width = std::max(
        static_cast<float>(map.columns * map.tile_width), kChrisWorldWidth * 2.0F);
    const float map_height = std::max(
        static_cast<float>(map.rows * map.tile_height), kChrisWorldHeight * 2.0F);
    const auto background = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background, svanes::Transform{kChrisWorldWidth * 0.5F,
                                      kChrisWorldHeight * 0.5F, 0.0F});
    context.world.AddComponent<svanes::SolidShape>(
        background, svanes::SolidShape{
            kP2PBackgroundColor,
            svanes::Rectangle2D{0.0F, 0.0F, map_width, map_height}});
    context.world.AddComponent<svanes::ZOrder>(background, svanes::ZOrder{0});
    context.world.AddComponent<svanes::ZOrder>(ground, svanes::ZOrder{1});
    simulation = std::make_unique<DoubleTimeP2PSimulation>(context, idle, running, ground);
    const svanes::SyncSettings sync_settings{.step_tics = kStepTics};
    const svanes::PeerSettings settings{kSession, 3, RulesHash(sync_settings)};
    auto pipe = std::make_unique<svanes::UdpMsgPipe>(port);
    network = entry ? std::make_unique<svanes::PeerGroup>(std::move(pipe), *entry, settings)
                    : std::make_unique<svanes::PeerGroup>(std::move(pipe), settings);
    sync = std::make_unique<svanes::InputSync<DoubleTimeP2PInput, DoubleTimeP2PInputUse>>(*network, *simulation, sync_settings);
}
void DoubleTimeP2PGame::Update(const svanes::FrameContext &frame) {
    if (frame.input.WasPressed(svanes::Key::Escape)) sync->RequestLeave();
    sync->Update(frame);
    simulation->UpdatePresentation(frame.real_delta_tics);
    should_quit = sync->CanClose();
    if (!network->IsRunning()) return;
    const auto status = "P2P peer " + std::to_string(network->LocalPeer().value) + ": " + sync->Status();
    if (status != last_status) { SDL_Log("%s", status.c_str()); last_status = status; }
    // Double Time's arena is fixed-size. Assign the camera center each frame;
    // accumulating an offset here would drift the entire world out of view.
    frame.camera.x = kChrisWorldWidth * 0.5F;
    frame.camera.y = kChrisWorldHeight * 0.5F;
}
