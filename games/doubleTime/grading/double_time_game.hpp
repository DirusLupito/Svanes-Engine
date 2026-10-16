#pragma once

#include "network_protocol.hpp"
#include "asset_catalog.hpp"
#include "rhythm_helpers.hpp"
#include "tilemap_parser.hpp"

#include <svanes/entity.hpp>
#include <svanes/audio/audio_manager.hpp>
#include <svanes/game.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/network/network_client.hpp>
#include <svanes/network/udp_msg_pipe.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/tilemaps/tilemap.hpp>

#include <string>
#include <map>
#include <memory>
#include <vector>

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

class DoubleTimeGame final : public svanes::IGame {
public:
    explicit DoubleTimeGame(
        std::string server_host = kDoubleTimeDefaultServerHost,
        std::uint16_t input_bridge_port = 0,
        bool input_bridge_controls_platforms = false);

    void Initialize(svanes::GameContext &context) override;
    void Update(const svanes::FrameContext &frame) override;

private:
    void EnterState(DoubleTimeState next, svanes::Registry &world);
    void EnterArea(std::size_t area_index, svanes::Registry &world);
    void LoadAreaAssets(std::size_t area_index);
    void ClearLevelEntities(svanes::Registry &world);
    void ActivateTilemap(svanes::Registry &world, std::size_t index);
    void CheckGoal(svanes::Registry &world);
    void SendInput(const svanes::FrameContext &frame);
    void PollInputBridge();
    void ApplyServerState(svanes::Registry &world);
    void SynchronizeAudio(float elapsed_seconds);
    void UpdateCamera(svanes::Camera2D &camera, svanes::Registry &world);

    svanes::NetworkClient network_client;
    std::unique_ptr<svanes::UdpMsgPipe> input_bridge_pipe;
    bool input_bridge_controls_platforms = false;
    bool has_bridge_input = false;
    DoubleTimeBridgeInput bridge_input{};
    svanes::Entity character_entity = 0;
    bool has_character = false;
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
    bool platform_action_requested_since_last_send = false;
    bool platform_left_was_down = false;
    bool platform_right_was_down = false;
    bool platform_up_was_down = false;
    bool platform_down_was_down = false;
    bool platform_left_on_beat = false;
    bool platform_right_on_beat = false;
    bool platform_up_on_beat = false;
    bool platform_down_on_beat = false;
    bool platform_action_on_beat = false;
    std::uint64_t platform_left_beat_index = 0;
    std::uint64_t platform_right_beat_index = 0;
    std::uint64_t platform_up_beat_index = 0;
    std::uint64_t platform_down_beat_index = 0;
    std::uint64_t platform_action_beat_index = 0;
    std::map<std::uint32_t, svanes::Entity> platform_entities;
    std::map<std::uint32_t, svanes::Entity> door_entities;
    std::map<std::uint32_t, svanes::Entity> spike_entities;
    std::map<std::uint32_t, svanes::Entity> collectible_entities;

    bool has_audio_sync = false;
    std::uint64_t synced_audio_tics = 0;
    float audio_sync_age_seconds = 0.0F;
    float applied_audio_rate = 1.0F;

};
