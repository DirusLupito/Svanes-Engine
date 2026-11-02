#pragma once

#include "collision_response.hpp"
#include "game_events.hpp"
#include "replay_server.hpp"
#include "tilemap_parser.hpp"

#include <svanes/network/clientserver/IServer.hpp>

#include <chrono>
#include <optional>
#include <vector>

class DoubleTimeServer final : public svanes::IServer {
public:
    void Initialize(svanes::ServerContext &context) override;
    svanes::JoinResponse
    HandleJoinRequest(const svanes::ServerJoinContext &context) override;
    void HandleDisconnect(svanes::Registry &world,
                          svanes::ClientId client) override;
    void Update(const svanes::ServerUpdateContext &context) override;
    void PhysicsUpdate(const svanes::ServerPhysicsContext &context) override;

private:
    enum class PlatformMotionPhase {
        Idle,
        MovingToEnd,
        Holding,
        MovingToStart
    };
    struct PlatformMotionTrack {
        PlatformMotionPhase phase = PlatformMotionPhase::Idle;
        float elapsed = 0.0F;
        float direction = 1.0F;
        bool nudge = false;
        std::optional<svanes::TicCount> return_tics;
    };
    struct ServerCharacter {
        svanes::Entity entity;
        svanes::ClientId client_id;
        float horizontal_input = 0.0F;
        bool jump_requested = false;
        bool jump_on_beat = false;
        float time_since_grounded = 0.0F;
    };
    struct ServerPlatform {
        DoubleTimeMovingPlatform definition;
        svanes::Entity entity;
        svanes::Transform start_transform;
        svanes::Transform end_transform;
        svanes::Transform previous_transform;
    };
    struct ServerDoor {
        DoubleTimeDoor definition;
        svanes::Entity entity;
        svanes::Transform transform;
        svanes::Transform base_transform;
        bool open = false;
        std::optional<svanes::TicCount> close_tics;
        float nudge_remaining_seconds = 0.0F;
    };
    struct ServerAreaObject {
        DoubleTimeLevelArea definition;
        svanes::Entity entity;
        bool enabled = true;
    };

    void RegisterHandlers();
    void HandleCommand(const DoubleTimeCommandEvent &event);
    void HandleSpawn(const DoubleTimePlayerSpawnEvent &event);
    void HandleRespawn();
    void HandleCollection(svanes::Entity entity);
    void AdvanceLevel();
    void LoadLevel(std::size_t area, std::size_t tilemap);
    void ClearLevel();
    void AdvanceControls();
    void BroadcastState(svanes::NetworkServer &network);
    DoubleTimeWorldSnapshotEvent CaptureSnapshot() const;
    void ReleasePlayer(svanes::ClientId client);
    void ResetPlatformControls();

    svanes::Registry *active_world = nullptr;
    svanes::EventManager *events = nullptr;
    svanes::Entity authoritative_timeline_entity = 0;
    svanes::Entity level_tilemap_entity = 0;
    bool has_level = false;
    svanes::Entity trigger_probe = 0;
    svanes::Transform tile_map_transform;
    float character_spawn_x = 20.0F;
    float character_spawn_y = 20.0F;
    std::size_t current_area_index = 0;
    std::size_t current_tilemap_index = 0;
    std::optional<svanes::Entity> goal_entity;
    std::vector<ServerPlatform> platforms;
    std::vector<ServerDoor> doors;
    std::vector<ServerAreaObject> death_zones;
    std::vector<ServerAreaObject> spikes;
    std::vector<ServerAreaObject> collectibles;
    std::optional<ServerCharacter> character;
    std::optional<svanes::ClientId> platform_client_id;
    bool platform_left_held = false;
    bool platform_right_held = false;
    bool platform_up_held = false;
    bool platform_down_held = false;
    bool input_left_on_beat = false;
    bool input_right_on_beat = false;
    bool input_up_on_beat = false;
    bool input_down_on_beat = false;
    bool input_action_on_beat = false;
    bool left_press_pending = false;
    bool right_press_pending = false;
    bool up_press_pending = false;
    bool down_press_pending = false;
    PlatformMotionTrack left_motion;
    PlatformMotionTrack right_motion;
    PlatformMotionTrack up_motion;
    PlatformMotionTrack down_motion;
    std::optional<std::size_t> character_platform_index;
    DoubleTimeCollisionHandler collisions;
    std::unique_ptr<DoubleTimeReplayServer> replay;
    std::chrono::steady_clock::time_point last_replay_tick;
};
