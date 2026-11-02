#include "double_time_server.hpp"
#include "asset_catalog.hpp"
#include "rhythm_helpers.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <svanes/asset_path.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/registry.hpp>

namespace {
constexpr float kCharacterMaxSpeedPerSecond = 547.0F * kDoubleTimeWorldScale;
constexpr float kCharacterAccelerationPerSecondSquared =
    2400.0F * kDoubleTimeWorldScale;
constexpr float kCharacterDecelerationPerSecondSquared =
    3200.0F * kDoubleTimeWorldScale;
constexpr float kJumpSpeedPerSecond = 1200.0F * kDoubleTimeWorldScale;
constexpr float kGravityPerSecondSquared = 3000.0F * kDoubleTimeWorldScale;
constexpr float kCoyoteTimeSeconds = 0.10F;
constexpr float kCharacterSpawnLeft = 20.0F;
constexpr float kCharacterSpawnTop = 20.0F;
constexpr float kCharacterColliderWidth = kDoubleTimeCharacterWidth * 0.8F;
constexpr float kPlatformTravelDurationSeconds = 0.12F;
constexpr std::uint32_t kPlatformHoldBeats = 4;
constexpr float kOffBeatJumpForceMultiplier = 0.2F;
}

void DoubleTimeServer::Initialize(svanes::ServerContext &context) {
    active_world = &context.world;
    events = &context.events;
    context.gravity = {0.0F, svanes::PerSecondSquaredToPerTicSquared(
                                 kGravityPerSecondSquared)};
    authoritative_timeline_entity = active_world->CreateEntity();
    active_world->AddComponent<svanes::Timeline>(authoritative_timeline_entity);
    trigger_probe = active_world->CreateEntity();
    active_world->AddComponent<svanes::Transform>(trigger_probe);
    active_world->AddComponent<svanes::Collider2D>(
        trigger_probe, svanes::Collider2D{svanes::Rectangle2D{
                           0.0F, 0.0F, kDoubleTimeCharacterWidth,
                           kDoubleTimeCharacterHeight}});
    RegisterHandlers();
    collisions.Register(context.world, context.events);
    LoadLevel(0, 0);
    char *preferences = SDL_GetPrefPath("Svanes", "DoubleTime");
    if (!preferences) {
        throw std::runtime_error(std::string{"Cannot locate replay storage: "} +
                                 SDL_GetError());
    }
    const auto replay_path = std::filesystem::path{preferences} / "replay.svr";
    SDL_free(preferences);
    replay = std::make_unique<DoubleTimeReplayServer>(
        replay_path,
        [&network = context.network](svanes::ClientId client,
                                     const svanes::NetworkMessage &message) {
            network.Send(client, message);
        },
        CaptureSnapshot().Write());
    last_replay_tick = std::chrono::steady_clock::now();
    context.network.Listen("*:" + std::to_string(kDoubleTimeServerJoinPort));
}

void DoubleTimeServer::RegisterHandlers() {
    events->Subscribe(
        DoubleTimeCommandEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<DoubleTimeCommandEvent>>(
            [this](const DoubleTimeCommandEvent &event) {
                HandleCommand(event);
            }));
    events->Subscribe(
        DoubleTimePlayerSpawnEvent::GetStaticEventType(),
        std::make_unique<
            svanes::EventHandlerWrapper<DoubleTimePlayerSpawnEvent>>(
            [this](const DoubleTimePlayerSpawnEvent &event) {
                HandleSpawn(event);
            }));
    events->Subscribe(
        DoubleTimePlayerDisconnectedEvent::GetStaticEventType(),
        std::make_unique<
            svanes::EventHandlerWrapper<DoubleTimePlayerDisconnectedEvent>>(
            [this](const DoubleTimePlayerDisconnectedEvent &event) {
                ReleasePlayer(event.client);
            }));
    events->Subscribe(
        DoubleTimeRespawnEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<DoubleTimeRespawnEvent>>(
            [this](const DoubleTimeRespawnEvent &) { HandleRespawn(); }));
    events->Subscribe(
        DoubleTimeCollectEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<DoubleTimeCollectEvent>>(
            [this](const DoubleTimeCollectEvent &event) {
                HandleCollection(event.entity);
            }));
    events->Subscribe(
        DoubleTimeAdvanceLevelEvent::GetStaticEventType(),
        std::make_unique<
            svanes::EventHandlerWrapper<DoubleTimeAdvanceLevelEvent>>(
            [this](const DoubleTimeAdvanceLevelEvent &) { AdvanceLevel(); }));
}

svanes::JoinResponse
DoubleTimeServer::HandleJoinRequest(const svanes::ServerJoinContext &) {
    return {true, {}};
}

void DoubleTimeServer::HandleDisconnect(svanes::Registry &,
                                        svanes::ClientId client) {
    replay->RemoveClient(client);
    events->TriggerEvent(DoubleTimePlayerDisconnectedEvent{client});
}

void DoubleTimeServer::HandleSpawn(const DoubleTimePlayerSpawnEvent &event) {
    if (character) {
        throw std::logic_error("Double Time already has a character.");
    }
    const auto entity = active_world->CreateEntity();
    active_world->AddComponent<svanes::Transform>(
        entity, svanes::Transform{character_spawn_x, character_spawn_y, 0.0F});
    active_world->AddComponent<svanes::Kinematic2D>(entity);
    active_world->AddComponent<svanes::Gravity>(entity);
    active_world->AddComponent<svanes::Collider2D>(
        entity,
        svanes::Collider2D{svanes::Rectangle2D{
            0.0F, 0.0F, kCharacterColliderWidth, kDoubleTimeCharacterHeight}});
    active_world->AddComponent<svanes::Timeline>(entity);
    character = ServerCharacter{entity, event.client};
    SDL_Log("Server: spawned the character for client %u.", event.client);
}

void DoubleTimeServer::HandleCommand(const DoubleTimeCommandEvent &event) {
    const auto &input = event.input;
    if (!std::isfinite(input.horizontal) || input.horizontal < -1.0F ||
        input.horizontal > 1.0F) {
        throw std::invalid_argument(
            "Double Time horizontal input must be between -1 and 1.");
    }
    if (!input.platform_role) {
        if (!character) {
            events->TriggerEvent(DoubleTimePlayerSpawnEvent{input.client_id});
        }
        if (character->client_id == input.client_id) {
            character->horizontal_input = input.horizontal;
            character->jump_requested =
                character->jump_requested || input.action_requested;
            character->jump_on_beat =
                character->jump_on_beat ||
                (input.action_requested && input.jump_on_beat);
        }
        return;
    }
    if (!platform_client_id) {
        platform_client_id = input.client_id;
        SDL_Log("Server: client %u controls moving platforms.",
                input.client_id);
    }
    if (*platform_client_id != input.client_id) {
        return;
    }
    const auto capture = [](bool down, bool was_down, bool on_beat,
                            bool &pending, bool &timing) {
        if (down && !was_down) {
            pending = true;
            timing = on_beat;
        }
    };
    capture(input.platform_left, platform_left_held,
            input.platform_left_on_beat, left_press_pending,
            input_left_on_beat);
    capture(input.platform_right, platform_right_held,
            input.platform_right_on_beat, right_press_pending,
            input_right_on_beat);
    capture(input.platform_up, platform_up_held, input.platform_up_on_beat,
            up_press_pending, input_up_on_beat);
    capture(input.platform_down, platform_down_held,
            input.platform_down_on_beat, down_press_pending,
            input_down_on_beat);
    platform_left_held = input.platform_left;
    platform_right_held = input.platform_right;
    platform_up_held = input.platform_up;
    platform_down_held = input.platform_down;
    input_action_on_beat = input.platform_action_on_beat;
    const auto server_tics =
        active_world
            ->GetComponent<svanes::Timeline>(authoritative_timeline_entity)
            .GetTotalTics();
    if (input.platform_action_requested) {
        for (auto &door : doors) {
            if (input_action_on_beat) {
                door.open = true;
                door.close_tics =
                    server_tics +
                    static_cast<svanes::TicCount>(std::llround(
                        double_time::BeatDurationTics(
                            DoubleTimeAssetCatalog()[current_area_index]
                                .beats_per_minute) *
                        kPlatformHoldBeats));
                door.nudge_remaining_seconds = 0.0F;
                door.transform = door.base_transform;
                active_world->RemoveComponent<svanes::Collider2D>(door.entity);
                active_world->RemoveComponent<DoubleTimeSolid>(door.entity);
            } else if (!door.open) {
                door.nudge_remaining_seconds = kPlatformTravelDurationSeconds;
                door.transform = door.base_transform;
                door.transform.x += kDoubleTimeTileSize * 0.15F;
            }
        }
    }
}

void DoubleTimeServer::ResetPlatformControls() {
    left_press_pending = right_press_pending = up_press_pending =
        down_press_pending = false;
    platform_left_held = platform_right_held = platform_up_held =
        platform_down_held = false;
    input_left_on_beat = input_right_on_beat = input_up_on_beat =
        input_down_on_beat = input_action_on_beat = false;
}

void DoubleTimeServer::ReleasePlayer(svanes::ClientId client) {
    if (character && character->client_id == client) {
        active_world->DestroyEntity(character->entity);
        character.reset();
        character_platform_index.reset();
    }
    if (platform_client_id == client) {
        platform_client_id.reset();
        ResetPlatformControls();
    }
}

void DoubleTimeServer::HandleRespawn() {
    if (!character) {
        return;
    }
    active_world->GetComponent<svanes::Transform>(character->entity) =
        svanes::Transform{character_spawn_x, character_spawn_y, 0.0F};
    auto &motion =
        active_world->GetComponent<svanes::Kinematic2D>(character->entity);
    motion.velocity_x = motion.velocity_y = 0.0F;
    character->time_since_grounded = 0.0F;
    character_platform_index.reset();
}

void DoubleTimeServer::HandleCollection(svanes::Entity entity) {
    const auto found = std::find_if(
        collectibles.begin(), collectibles.end(),
        [entity](const auto &object) { return object.entity == entity; });
    if (found == collectibles.end()) {
        throw std::invalid_argument("Unknown Double Time collectible.");
    }
    found->enabled = false;
    active_world->RemoveComponent<svanes::Collider2D>(entity);
}

void DoubleTimeServer::ClearLevel() {
    for (const auto &platform : platforms) {
        active_world->DestroyEntity(platform.entity);
    }
    for (const auto &door : doors) {
        active_world->DestroyEntity(door.entity);
    }
    for (const auto *objects : {&death_zones, &spikes, &collectibles}) {
        for (const auto &object : *objects) {
            active_world->DestroyEntity(object.entity);
        }
    }
    platforms.clear();
    doors.clear();
    death_zones.clear();
    spikes.clear();
    collectibles.clear();
    if (goal_entity) {
        active_world->DestroyEntity(*goal_entity);
        goal_entity.reset();
    }
    if (has_level) {
        active_world->DestroyEntity(level_tilemap_entity);
        has_level = false;
    }
    character_platform_index.reset();
}

void DoubleTimeServer::LoadLevel(std::size_t area, std::size_t tilemap) {
    const auto &definition = DoubleTimeAssetCatalog().at(area);
    auto level = LoadDoubleTimeLevelData(svanes::AssetPath(
        std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" +
        std::string{definition.tilemap_filenames.at(tilemap)}));
    ClearLevel();
    current_area_index = area;
    current_tilemap_index = tilemap;
    tile_map_transform = GetCenteredTileMapTransform(
        level.tile_map, kDoubleTimeWorldWidth * 0.5F,
        kDoubleTimeWorldHeight * 0.5F);
    character_spawn_x =
        tile_map_transform.x +
        (level.spawn_point ? (level.spawn_point->x + 0.5F) * kDoubleTimeTileSize
                           : kCharacterSpawnLeft);
    character_spawn_y =
        tile_map_transform.y +
        (level.spawn_point ? (level.spawn_point->y + 0.5F) * kDoubleTimeTileSize
                           : kCharacterSpawnTop);
    level_tilemap_entity = active_world->CreateEntity();
    active_world->AddComponent<svanes::Transform>(level_tilemap_entity,
                                                  tile_map_transform);
    active_world->AddComponent<svanes::TileMap>(level_tilemap_entity,
                                                std::move(level.tile_map));
    has_level = true;
    for (const auto &platform : level.moving_platforms) {
        const auto transform_at = [&](std::int32_t x, std::int32_t y) {
            return svanes::Transform{
                tile_map_transform.x +
                    (x + platform.width * 0.5F) * kDoubleTimeTileSize,
                tile_map_transform.y +
                    (y + platform.height * 0.5F) * kDoubleTimeTileSize,
                0.0F};
        };
        const auto start = transform_at(platform.start_x, platform.start_y);
        const auto end = transform_at(platform.end_x, platform.end_y);
        const auto entity = active_world->CreateEntity();
        active_world->AddComponent<svanes::Transform>(entity, start);
        active_world->AddComponent<svanes::Collider2D>(
            entity, svanes::Collider2D{svanes::Rectangle2D{
                        0.0F, 0.0F, platform.width * kDoubleTimeTileSize,
                        platform.height * kDoubleTimeTileSize}});
        active_world->AddComponent<DoubleTimeSolid>(entity);
        platforms.push_back({platform, entity, start, end, start});
    }
    for (const auto &door : level.doors) {
        const svanes::Transform transform{
            tile_map_transform.x +
                (door.x + door.width * 0.5F) * kDoubleTimeTileSize,
            tile_map_transform.y +
                (door.y + door.height * 0.5F) * kDoubleTimeTileSize,
            0.0F};
        const auto entity = active_world->CreateEntity();
        active_world->AddComponent<svanes::Transform>(entity, transform);
        active_world->AddComponent<svanes::Collider2D>(
            entity, svanes::Collider2D{svanes::Rectangle2D{
                        0.0F, 0.0F, door.width * kDoubleTimeTileSize,
                        door.height * kDoubleTimeTileSize}});
        active_world->AddComponent<DoubleTimeSolid>(entity);
        doors.push_back({door, entity, transform, transform});
    }
    const auto create_area = [&](const auto &definitions, auto &objects,
                                 DoubleTimeTrigger::Kind kind) {
        for (const auto &area : definitions) {
            const auto entity = active_world->CreateEntity();
            active_world->AddComponent<svanes::Transform>(
                entity,
                svanes::Transform{
                    tile_map_transform.x +
                        (area.x + area.width * 0.5F) * kDoubleTimeTileSize,
                    tile_map_transform.y +
                        (area.y + area.height * 0.5F) * kDoubleTimeTileSize,
                    0.0F});
            active_world->AddComponent<svanes::Collider2D>(
                entity, svanes::Collider2D{svanes::Rectangle2D{
                            0.0F, 0.0F, area.width * kDoubleTimeTileSize,
                            area.height * kDoubleTimeTileSize}});
            active_world->AddComponent<DoubleTimeTrigger>(
                entity, DoubleTimeTrigger{kind});
            objects.push_back(ServerAreaObject{area, entity});
        }
    };
    create_area(level.death_zones, death_zones,
                DoubleTimeTrigger::Kind::Hazard);
    create_area(level.spike_zones, spikes, DoubleTimeTrigger::Kind::Hazard);
    create_area(level.collectibles, collectibles,
                DoubleTimeTrigger::Kind::Collectible);
    if (level.goal) {
        std::vector<ServerAreaObject> goal;
        const auto &area = *level.goal;
        create_area(std::vector<DoubleTimeLevelArea>{{area.x, area.y,
                                                      area.width, area.height}},
                    goal, DoubleTimeTrigger::Kind::Goal);
        goal_entity = goal.front().entity;
    }
    left_motion = {};
    right_motion = {};
    up_motion = {};
    down_motion = {};
    ResetPlatformControls();
}

void DoubleTimeServer::AdvanceLevel() {
    const auto &area = DoubleTimeAssetCatalog().at(current_area_index);
    if (current_tilemap_index + 1 < area.tilemap_filenames.size()) {
        LoadLevel(current_area_index, current_tilemap_index + 1);
    } else if (area.next_area) {
        LoadLevel(*area.next_area, 0);
    } else {
        return;
    }
    events->TriggerEvent(DoubleTimeRespawnEvent{});
}

void DoubleTimeServer::Update(const svanes::ServerUpdateContext &context) {
    for (const auto &packet : context.network.PollInputs()) {
        replay->AddClient(packet.client);
        if (const auto kind = DoubleTimeReplayKind(packet.message)) {
            if (packet.message.bytes.size() != 5) {
                throw std::invalid_argument("Invalid replay request payload.");
            }
            replay->Request(packet.client, *kind);
            if (replay->IsViewing(packet.client)) {
                if (character && character->client_id == packet.client) {
                    character->horizontal_input = 0.0F;
                    character->jump_requested = false;
                    character->jump_on_beat = false;
                }
                if (platform_client_id == packet.client) {
                    ResetPlatformControls();
                }
            }
            continue;
        }
        if (replay->IsViewing(packet.client)) {
            continue;
        }
        auto command = packet.message.As<PlayerInputMessage>();
        command.client_id = packet.client;
        events->QueueEvent(std::make_unique<DoubleTimeCommandEvent>(command));
    }
    events->DispatchEvents();
    AdvanceControls();
}

void DoubleTimeServer::AdvanceControls() {
    if (character.has_value()) {
        svanes::Kinematic2D &motion =
            active_world->GetComponent<svanes::Kinematic2D>(character->entity);
        const float target_velocity =
            character->horizontal_input *
            svanes::PerSecondToPerTic(kCharacterMaxSpeedPerSecond);
        const float acceleration = character->horizontal_input == 0.0F
                                       ? kCharacterDecelerationPerSecondSquared
                                       : kCharacterAccelerationPerSecondSquared;
        const float max_velocity_change =
            svanes::PerSecondSquaredToPerTicSquared(acceleration) *
            static_cast<float>(svanes::DefaultPhysicsStepTics);
        motion.velocity_x =
            std::clamp(target_velocity, motion.velocity_x - max_velocity_change,
                       motion.velocity_x + max_velocity_change);
    }
    const float step_seconds =
        static_cast<float>(svanes::DefaultPhysicsStepTics) /
        static_cast<float>(svanes::TicsPerSecond);
    const svanes::TicCount server_tics =
        active_world
            ->GetComponent<svanes::Timeline>(authoritative_timeline_entity)
            .GetTotalTics();
    for (ServerDoor &door : doors) {
        if (door.nudge_remaining_seconds > 0.0F) {
            door.nudge_remaining_seconds -= step_seconds;
            if (door.nudge_remaining_seconds <= 0.0F) {
                door.nudge_remaining_seconds = 0.0F;
                door.transform = door.base_transform;
            }
        }
        if (door.open && door.close_tics.has_value() &&
            server_tics >= *door.close_tics) {
            door.open = false;
            door.close_tics.reset();
            door.transform = door.base_transform;
            active_world->AddComponent<svanes::Collider2D>(
                door.entity,
                svanes::Collider2D{svanes::Rectangle2D{
                    0.0F, 0.0F, door.definition.width * kDoubleTimeTileSize,
                    door.definition.height * kDoubleTimeTileSize}});
            active_world->AddComponent<DoubleTimeSolid>(door.entity,
                                                        DoubleTimeSolid{});
        }
    }
    const auto advance_motion = [&](PlatformMotionTrack &track, bool vertical) {
        if (track.phase != PlatformMotionPhase::Idle) {
            if (track.phase == PlatformMotionPhase::Holding) {
                if (track.return_tics.has_value() &&
                    server_tics >= *track.return_tics) {
                    track.elapsed = 0.0F;
                    track.phase = PlatformMotionPhase::MovingToStart;
                }
                return;
            }
            track.elapsed += step_seconds;
            const float progress = std::clamp(
                track.elapsed / kPlatformTravelDurationSeconds, 0.0F, 1.0F);
            const bool moving_to_end =
                track.phase == PlatformMotionPhase::MovingToEnd;
            for (ServerPlatform &platform : platforms) {
                const float movement_delta =
                    vertical
                        ? platform.end_transform.y - platform.start_transform.y
                        : platform.end_transform.x - platform.start_transform.x;
                if (track.direction * movement_delta <= 0.0F) {
                    continue;
                }
                const svanes::Transform &from =
                    moving_to_end
                        ? platform.start_transform
                        : (track.nudge
                               ? svanes::Transform{platform.start_transform.x +
                                                       (platform.end_transform
                                                            .x -
                                                        platform.start_transform
                                                            .x) *
                                                           0.15F,
                                                   platform.start_transform.y +
                                                       (platform.end_transform
                                                            .y -
                                                        platform.start_transform
                                                            .y) *
                                                           0.15F,
                                                   0.0F}
                               : platform.end_transform);
                const svanes::Transform &to =
                    moving_to_end
                        ? (track.nudge
                               ? svanes::Transform{platform.start_transform.x +
                                                       (platform.end_transform
                                                            .x -
                                                        platform.start_transform
                                                            .x) *
                                                           0.15F,
                                                   platform.start_transform.y +
                                                       (platform.end_transform
                                                            .y -
                                                        platform.start_transform
                                                            .y) *
                                                           0.15F,
                                                   0.0F}
                               : platform.end_transform)
                        : platform.start_transform;
                active_world->GetComponent<svanes::Transform>(platform.entity) =
                    svanes::Transform{from.x + (to.x - from.x) * progress,
                                      from.y + (to.y - from.y) * progress,
                                      0.0F};
            }
            if (progress >= 1.0F) {
                track.elapsed = 0.0F;
                if (moving_to_end && !track.nudge) {
                    track.phase = PlatformMotionPhase::Holding;
                } else if (moving_to_end && track.nudge) {
                    track.phase = PlatformMotionPhase::MovingToStart;
                } else {
                    track.phase = PlatformMotionPhase::Idle;
                    track.return_tics.reset();
                    track.nudge = false;
                }
            }
        }
    };
    advance_motion(left_motion, false);
    advance_motion(right_motion, false);
    advance_motion(up_motion, true);
    advance_motion(down_motion, true);
    const bool left_pressed = left_press_pending;
    const bool right_pressed = right_press_pending;
    const bool up_pressed = up_press_pending;
    const bool down_pressed = down_press_pending;
    left_press_pending = right_press_pending = up_press_pending =
        down_press_pending = false;
    const auto start_motion = [&](PlatformMotionTrack &track, bool pressed,
                                  bool on_beat, float direction) {
        if (!pressed || track.phase != PlatformMotionPhase::Idle) {
            return;
        }
        track.direction = direction;
        track.elapsed = 0.0F;
        track.nudge = !on_beat;
        track.return_tics =
            on_beat
                ? std::optional<
                      svanes::TicCount>{server_tics +
                                        static_cast<svanes::TicCount>(
                                            std::llround(
                                                double_time::BeatDurationTics(
                                                    DoubleTimeAssetCatalog()
                                                        [current_area_index]
                                                            .beats_per_minute) *
                                                kPlatformHoldBeats))}
                : std::nullopt;
        track.phase = PlatformMotionPhase::MovingToEnd;
    };
    start_motion(left_motion, left_pressed, input_left_on_beat, -1.0F);
    start_motion(right_motion, right_pressed, input_right_on_beat, 1.0F);
    start_motion(up_motion, up_pressed, input_up_on_beat, -1.0F);
    start_motion(down_motion, down_pressed, input_down_on_beat, 1.0F);
    if (character.has_value()) {
        svanes::Transform &character_transform =
            active_world->GetComponent<svanes::Transform>(character->entity);
        svanes::Kinematic2D &character_motion =
            active_world->GetComponent<svanes::Kinematic2D>(character->entity);
        if (character_platform_index.has_value() &&
            character_motion.velocity_y == 0.0F) {
            ServerPlatform &platform = platforms[*character_platform_index];
            const svanes::Transform current_transform =
                active_world->GetComponent<svanes::Transform>(platform.entity);
            character_transform.x +=
                current_transform.x - platform.previous_transform.x;
            character_transform.y +=
                current_transform.y - platform.previous_transform.y;
        }
    }
    for (ServerPlatform &platform : platforms) {
        platform.previous_transform =
            active_world->GetComponent<svanes::Transform>(platform.entity);
    }
}

void DoubleTimeServer::PhysicsUpdate(
    const svanes::ServerPhysicsContext &context) {
    collisions.BeginStep();
    if (character) {
        collisions.Resolve(*active_world, *events, context.parallel_for,
                           character->entity, level_tilemap_entity);
        active_world->GetComponent<svanes::Transform>(trigger_probe) =
            active_world->GetComponent<svanes::Transform>(character->entity);
        collisions.DetectTriggers(*active_world, *events, context.parallel_for,
                                  trigger_probe);
        bool hazard = false;
        bool goal = false;
        for (const auto entity : collisions.GetTriggers()) {
            const auto kind =
                active_world->GetComponent<DoubleTimeTrigger>(entity).kind;
            if (kind == DoubleTimeTrigger::Kind::Hazard) {
                hazard = true;
            } else if (kind == DoubleTimeTrigger::Kind::Collectible) {
                events->QueueEvent(
                    std::make_unique<DoubleTimeCollectEvent>(entity));
            } else if (kind == DoubleTimeTrigger::Kind::Goal) {
                goal = true;
            }
        }
        const auto &area = DoubleTimeAssetCatalog()[current_area_index];
        goal = goal &&
               (current_tilemap_index + 1 < area.tilemap_filenames.size() ||
                area.next_area.has_value());
        if (hazard) {
            events->QueueEvent(std::make_unique<DoubleTimeRespawnEvent>());
        } else if (goal) {
            events->QueueEvent(std::make_unique<DoubleTimeAdvanceLevelEvent>());
        }
        events->DispatchEvents();
        auto &motion =
            active_world->GetComponent<svanes::Kinematic2D>(character->entity);
        auto delta = svanes::DefaultPhysicsStepTics;
        for (const auto &step : context.entity_steps) {
            if (step.entity == character->entity) {
                delta = step.delta_tics;
                break;
            }
        }
        if (!hazard && !goal && collisions.IsGrounded()) {
            character->time_since_grounded = 0.0F;
        } else {
            character->time_since_grounded +=
                static_cast<float>(delta) /
                static_cast<float>(svanes::TicsPerSecond);
        }
        if (character->jump_requested &&
            character->time_since_grounded <= kCoyoteTimeSeconds) {
            const float speed =
                kJumpSpeedPerSecond *
                (character->jump_on_beat ? 1.0F : kOffBeatJumpForceMultiplier);
            motion.velocity_y = -svanes::PerSecondToPerTic(speed);
        }
        character_platform_index.reset();
        if (!hazard && !goal && motion.velocity_y == 0.0F &&
            collisions.GetSupport()) {
            for (std::size_t index = 0; index < platforms.size(); ++index) {
                if (platforms[index].entity == *collisions.GetSupport()) {
                    character_platform_index = index;
                    break;
                }
            }
        }
        character->jump_requested = false;
        character->jump_on_beat = false;
    }
    BroadcastState(context.network);
}

void DoubleTimeServer::BroadcastState(svanes::NetworkServer &network) {
    const auto snapshot = CaptureSnapshot();
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             now - last_replay_tick)
                             .count();
    last_replay_tick = now;
    replay->Tick(snapshot, static_cast<svanes::TicCount>(elapsed));
    network.Broadcast(snapshot.Write());
}

DoubleTimeWorldSnapshotEvent DoubleTimeServer::CaptureSnapshot() const {
    std::vector<DoubleTimeEntityState> objects;
    const auto broadcast = [&objects](const DoubleTimeEntityState &state) {
        objects.push_back(state);
    };
    if (character.has_value()) {
        const svanes::Transform transform =
            active_world->GetComponent<svanes::Transform>(character->entity);
        broadcast(DoubleTimeEntityState{
            transform, character->horizontal_input != 0.0F, 0,
            DoubleTimeEntityKind::Character, kDoubleTimeCharacterWidth,
            kDoubleTimeCharacterHeight});
    }
    for (const ServerPlatform &platform : platforms) {
        const svanes::Transform transform =
            active_world->GetComponent<svanes::Transform>(platform.entity);
        broadcast(DoubleTimeEntityState{
            transform, false, platform.definition.id,
            DoubleTimeEntityKind::MovingPlatform,
            platform.definition.width * kDoubleTimeTileSize,
            platform.definition.height * kDoubleTimeTileSize});
    }
    for (const ServerDoor &door : doors) {
        broadcast(DoubleTimeEntityState{
            door.transform, false, door.definition.id,
            DoubleTimeEntityKind::Door,
            door.definition.width * kDoubleTimeTileSize,
            door.definition.height * kDoubleTimeTileSize, 0, !door.open});
    }
    for (const ServerAreaObject &spike : spikes) {
        const auto transform =
            active_world->GetComponent<svanes::Transform>(spike.entity);
        broadcast(DoubleTimeEntityState{
            transform, false, static_cast<std::uint32_t>(spike.entity),
            DoubleTimeEntityKind::Spikes,
            spike.definition.width * kDoubleTimeTileSize,
            spike.definition.height * kDoubleTimeTileSize, 0, spike.enabled});
    }
    for (const ServerAreaObject &collectible : collectibles) {
        const auto transform =
            active_world->GetComponent<svanes::Transform>(collectible.entity);
        broadcast(DoubleTimeEntityState{
            transform, false, static_cast<std::uint32_t>(collectible.entity),
            DoubleTimeEntityKind::Collectible,
            collectible.definition.width * kDoubleTimeTileSize,
            collectible.definition.height * kDoubleTimeTileSize, 0,
            collectible.enabled});
    }
    broadcast(DoubleTimeEntityState{
        {},
        false,
        0,
        DoubleTimeEntityKind::AudioTimeline,
        0.0F,
        0.0F,
        active_world
            ->GetComponent<svanes::Timeline>(authoritative_timeline_entity)
            .GetTotalTics(),
        true,
        static_cast<std::uint32_t>(current_area_index),
        static_cast<std::uint32_t>(current_tilemap_index)});

    if (!character) {
        DoubleTimeEntityState absent{};
        absent.kind = DoubleTimeEntityKind::Character;
        absent.enabled = false;
        broadcast(absent);
    }
    return DoubleTimeWorldSnapshotEvent{std::move(objects)};
}
