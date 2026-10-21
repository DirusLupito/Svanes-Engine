#pragma once

#include "network_protocol.hpp"
#include "asset_catalog.hpp"
#include "rhythm_helpers.hpp"
#include "tilemap_parser.hpp"

#include <svanes/entity.hpp>
#include <svanes/audio/audio_manager.hpp>
#include <svanes/network/clientserver/IClient.hpp>
#include <svanes/network/clientserver/network_client.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/tilemaps/tilemap.hpp>

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <span>

namespace svanes {
class Camera2D;
class TextureManager;
}

enum class DoubleTimeState {
    MainMenu,
    LevelOne,
};

struct DoubleTimeLevelAssets {
    svanes::TextureHandle tile_set{};
    std::vector<DoubleTimeLevelData> levels;
    svanes::MusicHandle music{};
};

class DoubleTimeGame final : public svanes::IClient {
public:
    explicit DoubleTimeGame(
        std::string server_host = kDoubleTimeDefaultServerHost);

    void Initialize(svanes::ClientContext &context) override;
    svanes::NetworkMessage CreateJoinRequest() override;
    void HandleJoinResult(const svanes::JoinResult &result) override;
    void Update(const svanes::ClientFrameContext &frame) override;

private:
    void EnterState(DoubleTimeState next, svanes::Registry &world);
    void EnterArea(std::size_t area_index, svanes::Registry &world);
    void LoadAreaAssets(std::size_t area_index);
    void ClearLevelEntities(svanes::Registry &world);
    void ActivateTilemap(svanes::Registry &world, std::size_t index);
    void CheckGoal(svanes::Registry &world);
    void SendInput(const svanes::ClientFrameContext &frame);
    void SimulateClientCharacter(const svanes::ClientFrameContext &frame);
    void EnsureClientCharacter(svanes::Registry &world);
    void ApplyServerState(svanes::Registry &world,
                          std::span<const svanes::NetworkMessage> messages);
    void SynchronizeAudio(float elapsed_seconds);
    void UpdateCamera(svanes::Camera2D &camera, svanes::Registry &world);

    std::string server_host;
    std::uint32_t client_id = 0;
    svanes::NetworkClient *active_network = nullptr;
    svanes::Entity character_entity = 0;
    bool has_character = false;
    bool local_character_initialized = false;
    float local_velocity_x = 0.0F;
    float local_velocity_y = 0.0F;
    std::map<std::uint32_t, svanes::Entity> grading_character_entities;
    svanes::Timeline client_timeline;

    svanes::AudioManager *audio_manager = nullptr;
    DoubleTimeState state = DoubleTimeState::MainMenu;
    std::vector<DoubleTimeLevelAssets> area_assets;
    std::size_t active_area_index = 0;
    svanes::Entity active_tilemap_entity = 0;
    bool has_active_tilemap = false;
    std::size_t active_tilemap_index = 0;
    svanes::Entity background_entity = 0;
    svanes::TextureManager *asset_manager = nullptr;

    float input_send_timer = 0.0F;
    bool action_requested_since_last_send = false;
    bool jump_on_beat_since_last_send = false;
    std::map<std::uint32_t, svanes::Entity> platform_entities;
    std::map<std::uint32_t, svanes::Entity> door_entities;
    std::map<std::uint32_t, svanes::Entity> spike_entities;
    std::map<std::uint32_t, svanes::Entity> collectible_entities;

    bool has_audio_sync = false;
    std::uint64_t synced_audio_tics = 0;
    float audio_sync_age_seconds = 0.0F;
    float applied_audio_rate = 1.0F;

};
