#pragma once

#include "network_protocol.hpp"

#include <svanes/entity.hpp>
#include <svanes/audio/audio_manager.hpp>
#include <svanes/game.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/network/network_client.hpp>
#include <svanes/network/network_replication.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <string>

class ChrisGame final : public svanes::IGame {
public:
    explicit ChrisGame(std::string server_host = kChrisDefaultServerHost,
                       ClientRole role = ClientRole::Character);

    void Initialize(svanes::GameContext &context) override;
    void Update(const svanes::FrameContext &frame) override;

private:
    void SendInput(const svanes::FrameContext &frame);
    void ApplyServerState(svanes::Registry &world);
    void SmoothNetworkedTransforms(const svanes::FrameContext &frame);
    svanes::Entity SpawnCharacter(svanes::Registry &world,
                                  svanes::Transform initial_transform);
    svanes::Entity SpawnPlatform(svanes::Registry &world,
                                 svanes::Transform initial_transform);

    svanes::NetworkClient network_client;
    svanes::NetworkEntityMap entity_map;
    ClientRole role;

    svanes::TextureHandle idle_texture{};
    svanes::TextureHandle running_texture{};
    svanes::MusicHandle music{};

    float input_send_timer = 0.0F;
    bool action_requested_since_last_send = false;

    bool character_is_running = false;
};
