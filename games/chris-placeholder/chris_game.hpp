#pragma once

#include "network_protocol.hpp"

#include <svanes/entity.hpp>
#include <svanes/game.hpp>
#include <svanes/network/network_client.hpp>
#include <svanes/network/network_replication.hpp>

#include <string>

class ChrisGame final : public svanes::IGame {
public:
    explicit ChrisGame(std::string server_host = kChrisDefaultServerHost);

    void Initialize(svanes::GameContext &context) override;
    void Update(const svanes::FrameContext &frame) override;

private:
    void SendInput(const svanes::FrameContext &frame);
    void ApplyServerState(svanes::Registry &world);
    svanes::Entity SpawnRemoteCharacter(svanes::Registry &world, svanes::Entity network_entity);

    svanes::NetworkClient network_client;
    svanes::NetworkEntityMap entity_map;

    float input_send_timer = 0.0F;
    bool jump_requested_since_last_send = false;
};
