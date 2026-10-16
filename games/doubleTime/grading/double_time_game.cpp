#include "double_time_game.hpp"
#include "tilemap_parser.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/timeline_system.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <array>
#include <stdexcept>
#include <utility>

namespace {

constexpr float kInputSendIntervalSeconds = 1.0F / 60.0F;
constexpr float kDoubleTimeMusicVolume = 0.15F;
constexpr float kAudioHardSyncThresholdMilliseconds = 250.0F;
constexpr float kAudioSoftSyncThresholdMilliseconds = 20.0F;
constexpr float kAudioMaxRateCorrection = 0.05F;
constexpr float kAudioRateCorrectionGain = 0.0005F;
constexpr double kRhythmTolerance = 0.175;

constexpr std::int32_t kGroundZOrder = 1;
constexpr std::int32_t kCharacterZOrder = 2;
constexpr std::int32_t kPlatformZOrder = 2;

constexpr svanes::Color kBackgroundColor{17, 24, 39, 255};

constexpr const char *kTestTileSetFilename = "TestTileSet.png";

} // namespace

DoubleTimeGame::DoubleTimeGame(std::string server_host,
                               std::uint16_t input_bridge_port,
                               bool input_bridge_controls_platforms)
    : network_client(std::move(server_host), kDoubleTimeServerJoinPort),
      input_bridge_controls_platforms(input_bridge_controls_platforms) {
    if (input_bridge_port != 0) {
        input_bridge_pipe =
            std::make_unique<svanes::UdpMsgPipe>(input_bridge_port);
    }
    SDL_Log("Networking: joining Double Time server.");
}

void DoubleTimeGame::EnterState(DoubleTimeState next,
                                svanes::Registry &world) {
    state = next;
    if (next == DoubleTimeState::MainMenu) {
        if (audio_manager != nullptr) {
            audio_manager->StopMusic();
        }
        return;
    }

    EnterArea(active_area_index, world);
}

void DoubleTimeGame::LoadAreaAssets(std::size_t area_index) {
    if (area_index >= DoubleTimeAssetCatalog().size()) {
        throw std::out_of_range("Double Time area index is out of range.");
    }
    if (area_assets[area_index].levels.empty()) {
        const auto &definition = DoubleTimeAssetCatalog()[area_index];
        auto &assets = area_assets[area_index];
        assets.music = audio_manager->LoadMusic(
            std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" +
            std::string{definition.music_filename});
        for (const std::string_view filename : definition.tilemap_filenames) {
            assets.levels.push_back(LoadDoubleTimeLevelData(
                std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" +
                    std::string{filename},
                assets.tile_set));
        }
    }
}

void DoubleTimeGame::EnterArea(std::size_t area_index,
                               svanes::Registry &world) {
    const bool area_changed = active_area_index != area_index ||
                               area_assets[area_index].levels.empty();
    LoadAreaAssets(area_index);
    active_area_index = area_index;
    if (audio_manager != nullptr) {
        audio_manager->SetMusicVolume(kDoubleTimeMusicVolume);
        audio_manager->SetMusicPlaybackRate(1.0F);
        if (area_changed) {
            audio_manager->PlayMusic(area_assets[active_area_index].music);
        }
    }
    ClearLevelEntities(world);
    ActivateTilemap(world, 0);
}

void DoubleTimeGame::ClearLevelEntities(svanes::Registry &world) {
    if (has_active_tilemap) {
        world.DestroyEntity(active_tilemap_entity);
        has_active_tilemap = false;
    }
    for (const auto &[id, entity] : platform_entities) {
        (void)id;
        world.DestroyEntity(entity);
    }
    for (const auto &[id, entity] : door_entities) {
        (void)id;
        world.DestroyEntity(entity);
    }
    platform_entities.clear();
    door_entities.clear();
    for (const auto &[id, entity] : spike_entities) {
        (void)id;
        world.DestroyEntity(entity);
    }
    for (const auto &[id, entity] : collectible_entities) {
        (void)id;
        world.DestroyEntity(entity);
    }
    spike_entities.clear();
    collectible_entities.clear();
    if (has_character) {
        world.DestroyEntity(character_entity);
        has_character = false;
    }
    for (const auto &[id, entity] : grading_character_entities) {
        (void)id;
        if (entity != character_entity) {
            world.DestroyEntity(entity);
        }
    }
    grading_character_entities.clear();
}

void DoubleTimeGame::ActivateTilemap(svanes::Registry &world,
                                     std::size_t index) {
    if (index >= area_assets[active_area_index].levels.size()) {
        throw std::out_of_range("Double Time tilemap index is out of range.");
    }
    if (has_active_tilemap) {
        world.DestroyEntity(active_tilemap_entity);
    }

    active_tilemap_entity = world.CreateEntity();
    const svanes::TileMap &tilemap = area_assets[active_area_index]
                                          .levels[index]
                                          .tile_map;
    world.AddComponent<svanes::Transform>(
        active_tilemap_entity,
        GetCenteredTileMapTransform(tilemap, kDoubleTimeWorldWidth * 0.5F,
                                    kDoubleTimeWorldHeight * 0.5F));
    world.AddComponent<svanes::TileMap>(active_tilemap_entity, tilemap);
    world.AddComponent<svanes::ZOrder>(
        active_tilemap_entity, svanes::ZOrder{kGroundZOrder});
    const float tilemap_width =
        static_cast<float>(tilemap.columns) * tilemap.tile_width;
    const float tilemap_height =
        static_cast<float>(tilemap.rows) * tilemap.tile_height;
    const auto &tilemap_transform =
        world.GetComponent<svanes::Transform>(active_tilemap_entity);
    const float background_width =
        std::max(kDoubleTimeWorldWidth, tilemap_width);
    world.GetComponent<svanes::Transform>(background_entity) =
        svanes::Transform{tilemap_transform.x + tilemap_width * 0.5F,
                           tilemap_transform.y + tilemap_height * 0.5F, 0.0F};
    world.GetComponent<svanes::SolidShape>(background_entity) =
        svanes::SolidShape{
            kBackgroundColor,
            svanes::Rectangle2D{0.0F, 0.0F, background_width,
                                std::max(kDoubleTimeWorldHeight,
                                         tilemap_height)}};
    active_tilemap_index = index;
    has_active_tilemap = true;
}

void DoubleTimeGame::CheckGoal(svanes::Registry &world) {
    if (!has_character || !has_active_tilemap ||
        active_area_index >= DoubleTimeAssetCatalog().size()) {
        return;
    }
    const auto &levels = area_assets[active_area_index].levels;
    if (active_tilemap_index >= levels.size() || !levels[active_tilemap_index].goal) {
        return;
    }
    const auto &goal = *levels[active_tilemap_index].goal;
    const auto &tilemap_transform =
        world.GetComponent<svanes::Transform>(active_tilemap_entity);
    const auto &character_transform =
        world.GetComponent<svanes::Transform>(character_entity);
    const float left = tilemap_transform.x + goal.x * kDoubleTimeTileSize;
    const float right = left + goal.width * kDoubleTimeTileSize;
    const float top = tilemap_transform.y + goal.y * kDoubleTimeTileSize;
    const float bottom = top + goal.height * kDoubleTimeTileSize;
    const float character_left =
        character_transform.x - kDoubleTimeCharacterWidth * 0.5F;
    const float character_right =
        character_transform.x + kDoubleTimeCharacterWidth * 0.5F;
    const float character_top =
        character_transform.y - kDoubleTimeCharacterHeight * 0.5F;
    const float character_bottom =
        character_transform.y + kDoubleTimeCharacterHeight * 0.5F;
    if (character_right > left && character_left < right &&
        character_bottom > top && character_top < bottom) {
        if (active_tilemap_index + 1 < levels.size()) {
            ClearLevelEntities(world);
            ActivateTilemap(world, active_tilemap_index + 1);
        } else if (const auto next_area =
                       DoubleTimeAssetCatalog()[active_area_index].next_area;
                   next_area.has_value()) {
            EnterArea(*next_area, world);
        }
    }
}

void DoubleTimeGame::Initialize(svanes::GameContext &context) {
    asset_manager = &context.assets;
    audio_manager = &context.audio;
    context.camera.scale_mode = svanes::ScaleMode::Proportional;
    context.camera.zoom = context.camera.Viewport().height /
                          context.camera.Scale() / kDoubleTimeWorldHeight;

    background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background_entity, svanes::Transform{kDoubleTimeWorldWidth * 0.5F,
                                             kDoubleTimeWorldHeight * 0.5F, 0.0F});
    context.world.AddComponent<svanes::SolidShape>(
        background_entity,
        svanes::SolidShape{kBackgroundColor,
                           svanes::Rectangle2D{0.0F, 0.0F, kDoubleTimeWorldWidth,
                                               kDoubleTimeWorldHeight}});

    area_assets.resize(DoubleTimeAssetCatalog().size());
    for (auto &area : area_assets) {
        area.tile_set = context.assets.LoadTexture(
        std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" + kTestTileSetFilename);
    }
    EnterState(DoubleTimeState::LevelOne, context.world);
}

void DoubleTimeGame::SendInput(const svanes::FrameContext &frame) {
    if (!network_client.IsConnected()) return;
    const auto assigned_id = network_client.Id();
    input_send_timer += static_cast<float>(client_timeline.GetDeltaTics()) /
                        static_cast<float>(svanes::TicsPerSecond);
    while (input_send_timer >= kInputSendIntervalSeconds) {
        input_send_timer -= kInputSendIntervalSeconds;

        float horizontal_input = 0.0F;
        bool jump_requested = action_requested_since_last_send;
        bool jump_on_beat = jump_on_beat_since_last_send;
        bool platform_left = frame.input.IsDown(svanes::Key::Left);
        bool platform_right = frame.input.IsDown(svanes::Key::Right);
        bool platform_up = frame.input.IsDown(svanes::Key::Up);
        bool platform_down = frame.input.IsDown(svanes::Key::Down);
        bool platform_action = platform_action_requested_since_last_send;
        if (input_bridge_pipe != nullptr) {
            if (has_bridge_input) {
                horizontal_input += bridge_input.character_horizontal;
                jump_requested = jump_requested || bridge_input.character_jump;
                jump_on_beat = jump_on_beat ||
                               (bridge_input.character_jump &&
                                double_time::IsOnBeat(
                                    client_timeline.GetTotalTics(),
                                    DoubleTimeAssetCatalog()[active_area_index]
                                        .beats_per_minute,
                                    kRhythmTolerance));
                platform_left = platform_left || bridge_input.platform_left;
                platform_right = platform_right || bridge_input.platform_right;
                platform_up = platform_up || bridge_input.platform_up;
                platform_down = platform_down || bridge_input.platform_down;
                platform_action = platform_action || bridge_input.platform_action;
            }
        }
        if (frame.input.IsDown(svanes::Key::A)) {
            horizontal_input -= 1.0F;
        }
        if (frame.input.IsDown(svanes::Key::D)) {
            horizontal_input += 1.0F;
        }

        const PlayerInputMessage input{assigned_id, horizontal_input,
                                       jump_requested, jump_on_beat,
                                       input_bridge_controls_platforms,
                                       platform_left,
                                       platform_right, platform_up, platform_down,
                                       platform_action,
                                       platform_left_on_beat,
                                       platform_right_on_beat,
                                       platform_up_on_beat,
                                       platform_down_on_beat,
                                       platform_action_on_beat,
                                       platform_left_beat_index,
                                       platform_right_beat_index,
                                       platform_up_beat_index,
                                       platform_down_beat_index,
                                       platform_action_beat_index};
        if constexpr (kDoubleTimeNetworkingStrategy ==
                      DoubleTimeNetworkingStrategy::InputAuthoritative) {
            network_client.Send(input);
        }
        action_requested_since_last_send = false;
        jump_on_beat_since_last_send = false;
        platform_action_requested_since_last_send = false;
    }
}

void DoubleTimeGame::PollInputBridge() {
    if (input_bridge_pipe == nullptr) {
        return;
    }
    svanes::NetworkMessage message;
    while (input_bridge_pipe->Receive(message)) {
        const DoubleTimeBridgeInput previous = bridge_input;
        bridge_input = message.As<DoubleTimeBridgeInput>();
        if (bridge_input.character_jump) {
            jump_on_beat_since_last_send =
                jump_on_beat_since_last_send ||
                double_time::IsOnBeat(
                    client_timeline.GetTotalTics(),
                    DoubleTimeAssetCatalog()[active_area_index].beats_per_minute,
                    kRhythmTolerance);
        }
        const auto capture = [&](bool down, bool was_down, bool &on_beat,
                                 std::uint64_t &beat_index) {
            if (down && !was_down) {
                beat_index = double_time::BeatIndex(
                    client_timeline.GetTotalTics(),
                    DoubleTimeAssetCatalog()[active_area_index].beats_per_minute);
                on_beat = double_time::IsOnBeat(
                    client_timeline.GetTotalTics(),
                    DoubleTimeAssetCatalog()[active_area_index].beats_per_minute,
                    kRhythmTolerance);
            }
        };
        capture(bridge_input.platform_left, previous.platform_left,
                platform_left_on_beat, platform_left_beat_index);
        capture(bridge_input.platform_right, previous.platform_right,
                platform_right_on_beat, platform_right_beat_index);
        capture(bridge_input.platform_up, previous.platform_up,
                platform_up_on_beat, platform_up_beat_index);
        capture(bridge_input.platform_down, previous.platform_down,
                platform_down_on_beat, platform_down_beat_index);
        capture(bridge_input.platform_action, previous.platform_action,
                platform_action_on_beat, platform_action_beat_index);
        has_bridge_input = true;
    }
}

void DoubleTimeGame::ApplyServerState(svanes::Registry &world) {
    const auto messages = network_client.PollBroadcast();
    for (const svanes::NetworkMessage &message : messages) {
        const DoubleTimeEntityState state = message.As<DoubleTimeEntityState>();
        if (state.kind != DoubleTimeEntityKind::AudioTimeline) {
            continue;
        }
        synced_audio_tics = state.timeline_tics;
        client_timeline.SetProgress(state.timeline_tics, 0,
                                    svanes::RationalNumber{0});
        audio_sync_age_seconds = 0.0F;
        has_audio_sync = true;
        const std::size_t server_area = state.area_index;
        const std::size_t server_tilemap = state.tilemap_index;
        if (server_area < DoubleTimeAssetCatalog().size()) {
            LoadAreaAssets(server_area);
            if (active_area_index != server_area) {
                EnterArea(server_area, world);
            } else if (!has_active_tilemap ||
                       active_tilemap_index != server_tilemap) {
                ClearLevelEntities(world);
                ActivateTilemap(world, server_tilemap);
            }
        }
    }

    for (const svanes::NetworkMessage &message : messages) {
        const DoubleTimeEntityState state = message.As<DoubleTimeEntityState>();

        if (state.kind == DoubleTimeEntityKind::AudioTimeline) {
            synced_audio_tics = state.timeline_tics;
            audio_sync_age_seconds = 0.0F;
            has_audio_sync = true;
            continue;
        }

        if (state.kind == DoubleTimeEntityKind::MovingPlatform) {
            auto found = platform_entities.find(state.entity_id);
            if (found == platform_entities.end()) {
                const svanes::Entity entity = world.CreateEntity();
                world.AddComponent<svanes::Transform>(entity, state.transform);
                world.AddComponent<svanes::SolidShape>(
                    entity,
                    svanes::SolidShape{
                        svanes::Color{110, 116, 134, 255},
                        svanes::Rectangle2D{0.0F, 0.0F, state.width,
                                            state.height}});
                world.AddComponent<svanes::ZOrder>(
                    entity, svanes::ZOrder{kPlatformZOrder});
                platform_entities.emplace(state.entity_id, entity);
            } else {
                world.GetComponent<svanes::Transform>(found->second) =
                    state.transform;
            }
            continue;
        }

        if (state.kind == DoubleTimeEntityKind::Door) {
            auto found = door_entities.find(state.entity_id);
            if (!state.enabled) {
                if (found != door_entities.end()) {
                    world.DestroyEntity(found->second);
                    door_entities.erase(found);
                }
                continue;
            }
            if (found == door_entities.end()) {
                const svanes::Entity entity = world.CreateEntity();
                world.AddComponent<svanes::Transform>(entity, state.transform);
                world.AddComponent<svanes::SolidShape>(
                    entity,
                    svanes::SolidShape{
                        svanes::Color{196, 120, 30, 255},
                        svanes::Rectangle2D{0.0F, 0.0F, state.width,
                                            state.height}});
                world.AddComponent<svanes::ZOrder>(
                    entity, svanes::ZOrder{kPlatformZOrder});
                door_entities.emplace(state.entity_id, entity);
            } else {
                world.GetComponent<svanes::Transform>(found->second) =
                    state.transform;
            }
            continue;
        }

        auto &special_entities = state.kind == DoubleTimeEntityKind::Spikes
            ? spike_entities : collectible_entities;
        if (state.kind == DoubleTimeEntityKind::Spikes ||
            state.kind == DoubleTimeEntityKind::Collectible) {
            auto found = special_entities.find(state.entity_id);
            if (!state.enabled) {
                if (found != special_entities.end()) {
                    world.DestroyEntity(found->second);
                    special_entities.erase(found);
                }
                continue;
            }
            if (found == special_entities.end()) {
                const svanes::Entity entity = world.CreateEntity();
                world.AddComponent<svanes::Transform>(entity, state.transform);
                world.AddComponent<svanes::SolidShape>(
                    entity, svanes::SolidShape{
                        state.kind == DoubleTimeEntityKind::Spikes
                            ? svanes::Color{220, 80, 20, 255}
                            : svanes::Color{255, 220, 0, 255},
                        svanes::Rectangle2D{0.0F, 0.0F, state.width,
                                            state.height}});
                world.AddComponent<svanes::ZOrder>(
                    entity, svanes::ZOrder{kPlatformZOrder});
                special_entities.emplace(state.entity_id, entity);
            }
            continue;
        }

        auto found_character = grading_character_entities.find(state.entity_id);
        if (found_character == grading_character_entities.end()) {
            const svanes::Entity entity = world.CreateEntity();
            world.AddComponent<svanes::Transform>(entity, state.transform);
            world.AddComponent<svanes::SolidShape>(
                entity,
                svanes::SolidShape{svanes::Color{76, 175, 80, 255},
                                   svanes::Rectangle2D{
                                       0.0F, 0.0F, kDoubleTimeCharacterWidth,
                                       kDoubleTimeCharacterHeight}});
            world.AddComponent<svanes::Timeline>(entity);
            world.AddComponent<svanes::ZOrder>(
                entity, svanes::ZOrder{kCharacterZOrder});
            grading_character_entities.emplace(state.entity_id, entity);
            if (state.entity_id == network_client.Id()) {
                character_entity = entity;
                has_character = true;
            }
        } else {
            world.GetComponent<svanes::Transform>(found_character->second) =
                state.transform;
        }
    }
}

void DoubleTimeGame::SynchronizeAudio(float elapsed_seconds) {
    if (audio_manager == nullptr || !has_audio_sync) {
        return;
    }

    audio_sync_age_seconds += elapsed_seconds;
    const std::int64_t duration_milliseconds =
        audio_manager->MusicDurationMilliseconds();
    if (duration_milliseconds <= 0) {
        return;
    }

    const double server_position_milliseconds =
        static_cast<double>(synced_audio_tics) / 1000.0 +
        static_cast<double>(audio_sync_age_seconds) * 1000.0;
    const double duration = static_cast<double>(duration_milliseconds);
    const double desired_position =
        std::fmod(server_position_milliseconds, duration);
    const double current_position = static_cast<double>(
        audio_manager->MusicPositionMilliseconds());
    double error = desired_position - current_position;
    if (error > duration * 0.5) {
        error -= duration;
    } else if (error < -duration * 0.5) {
        error += duration;
    }

    if (std::abs(error) >= kAudioHardSyncThresholdMilliseconds) {
        audio_manager->SeekMusic(static_cast<std::int64_t>(desired_position));
        if (applied_audio_rate != 1.0F) {
            audio_manager->SetMusicPlaybackRate(1.0F);
            applied_audio_rate = 1.0F;
        }
        return;
    }

    float target_rate = 1.0F;
    if (std::abs(error) >= kAudioSoftSyncThresholdMilliseconds) {
        target_rate = std::clamp(
            1.0F + static_cast<float>(error) * kAudioRateCorrectionGain,
            1.0F - kAudioMaxRateCorrection,
            1.0F + kAudioMaxRateCorrection);
    }
    if (std::abs(target_rate - applied_audio_rate) > 0.001F) {
        audio_manager->SetMusicPlaybackRate(target_rate);
        applied_audio_rate = target_rate;
    }
}

void DoubleTimeGame::UpdateCamera(svanes::Camera2D &camera,
                                  svanes::Registry &world) {
    if (!has_active_tilemap) {
        camera.x = kDoubleTimeWorldWidth * 0.5F;
        camera.y = kDoubleTimeWorldHeight * 0.5F;
        return;
    }

    const auto &tilemap = world.GetComponent<svanes::TileMap>(active_tilemap_entity);
    const auto &tilemap_transform =
        world.GetComponent<svanes::Transform>(active_tilemap_entity);
    const float tilemap_width =
        static_cast<float>(tilemap.columns) * tilemap.tile_width;
    const float tilemap_height =
        static_cast<float>(tilemap.rows) * tilemap.tile_height;
    const float view_width =
        camera.Viewport().width / (camera.Scale() * camera.zoom);
    const float view_height =
        camera.Viewport().height / (camera.Scale() * camera.zoom);

    float target_x = tilemap_transform.x + tilemap_width * 0.5F;
    if (has_character) {
        const auto &character_transform =
            world.GetComponent<svanes::Transform>(character_entity);
        target_x = character_transform.x;
    }

    const float min_camera_x = tilemap_transform.x + view_width * 0.5F;
    const float max_camera_x = tilemap_transform.x + tilemap_width -
                               view_width * 0.5F;
    camera.x = min_camera_x <= max_camera_x
                   ? std::clamp(target_x, min_camera_x, max_camera_x)
                   : tilemap_transform.x + tilemap_width * 0.5F;
    const float target_y = kDoubleTimeWorldHeight * 0.5F;
    const float min_camera_y = tilemap_transform.y + view_height * 0.5F;
    const float max_camera_y = tilemap_transform.y + tilemap_height -
                               view_height * 0.5F;
    camera.y = min_camera_y <= max_camera_y
                   ? std::clamp(target_y, min_camera_y, max_camera_y)
                   : tilemap_transform.y + tilemap_height * 0.5F;
}

void DoubleTimeGame::Update(const svanes::FrameContext &frame) {
    client_timeline.Advance(frame.real_delta_tics);
    PollInputBridge();
    const auto capture = [&](bool down, bool &was_down, bool &on_beat,
                             std::uint64_t &beat_index) {
        if (down && !was_down) {
            beat_index = double_time::BeatIndex(
                client_timeline.GetTotalTics(),
                DoubleTimeAssetCatalog()[active_area_index].beats_per_minute);
            on_beat = double_time::IsOnBeat(
                client_timeline.GetTotalTics(),
                DoubleTimeAssetCatalog()[active_area_index].beats_per_minute,
                kRhythmTolerance);
        }
        was_down = down;
    };
    capture(frame.input.IsDown(svanes::Key::Left), platform_left_was_down,
            platform_left_on_beat, platform_left_beat_index);
    capture(frame.input.IsDown(svanes::Key::Right), platform_right_was_down,
            platform_right_on_beat, platform_right_beat_index);
    capture(frame.input.IsDown(svanes::Key::Up), platform_up_was_down,
            platform_up_on_beat, platform_up_beat_index);
    capture(frame.input.IsDown(svanes::Key::Down), platform_down_was_down,
            platform_down_on_beat, platform_down_beat_index);
    if (frame.input.WasPressed(svanes::Key::Up) ||
        frame.input.WasPressed(svanes::Key::W)) {
        action_requested_since_last_send = true;
        jump_on_beat_since_last_send = double_time::IsOnBeat(
            client_timeline.GetTotalTics(),
            DoubleTimeAssetCatalog()[active_area_index].beats_per_minute,
            kRhythmTolerance);
    }
    if (frame.input.WasPressed(svanes::Key::Space)) {
        platform_action_requested_since_last_send = true;
        platform_action_beat_index = double_time::BeatIndex(
            client_timeline.GetTotalTics(),
            DoubleTimeAssetCatalog()[active_area_index].beats_per_minute);
        platform_action_on_beat = double_time::IsOnBeat(
            client_timeline.GetTotalTics(),
            DoubleTimeAssetCatalog()[active_area_index].beats_per_minute,
            kRhythmTolerance);
    }

    SendInput(frame);
    ApplyServerState(frame.world);
    CheckGoal(frame.world);
    SynchronizeAudio(static_cast<float>(frame.real_delta_tics) /
                     static_cast<float>(svanes::TicsPerSecond));

    UpdateCamera(frame.camera, frame.world);
}
