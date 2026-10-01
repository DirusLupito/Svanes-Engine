#pragma once

#include "network_protocol.hpp"

#include <svanes/game.hpp>
#include <svanes/network/input_sync.hpp>
#include <svanes/network/peer_group.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <cstdint>

inline constexpr std::uint16_t kDoubleTimeP2PHostPort = 5560;
inline constexpr std::uint16_t kDoubleTimeP2PJoinPort = 5561;

struct DoubleTimeP2PInput {
    float horizontal = 0.0F;
    bool jump = false;
};

struct DoubleTimeP2PInputUse {};

class DoubleTimeP2PSimulation final
    : public svanes::SyncedSimulation<DoubleTimeP2PInput,
                                      DoubleTimeP2PInputUse> {
public:
    explicit DoubleTimeP2PSimulation(svanes::GameContext &context,
                                     svanes::TextureHandle idle_texture,
                                     svanes::TextureHandle running_texture,
                                     svanes::Entity tilemap);

    svanes::NetworkMessage Save() const override;
    void Load(std::span<const std::byte> saved) override;
    std::vector<DoubleTimeP2PInputUse> Step(
        std::span<const DoubleTimeP2PInput> inputs) override;
    void CaptureInput(const svanes::FrameContext &frame,
                      svanes::PeerId local) override;
    DoubleTimeP2PInput TakeInput() override;
    void EncodeInput(svanes::MessageWriter &writer,
                     const DoubleTimeP2PInput &input) const override;
    DoubleTimeP2PInput DecodeInput(svanes::MessageReader &reader) const override;
    void AddPlayer(svanes::PeerId peer) override;
    void RemovePlayer(svanes::PeerId peer) override;
    svanes::Entity PlayerEntity(svanes::PeerId peer) const;
    void UpdatePresentation(svanes::TicCount real_delta_tics);

private:
    struct Player {
        svanes::PeerId peer;
        svanes::Entity entity;
        float x;
        float y;
        float horizontal_input = 0.0F;
        float velocity_y = 0.0F;
        bool grounded = false;
        bool grounded_on_platform = false;
    };

    enum class PlatformState : std::uint8_t {
        Idle,
        SlidingLeft,
        Holding,
        SlidingBack,
    };

    svanes::Registry &world;
    svanes::TextureHandle idle_texture;
    svanes::TextureHandle running_texture;
    svanes::Entity tilemap;
    std::vector<Player> players;
    svanes::Entity platform;
    float platform_x = kChrisPlatformLeft + kChrisPlatformWidth * 0.5F;
    PlatformState platform_state = PlatformState::Idle;
    svanes::TicCount platform_holding_tics = 0;
    DoubleTimeP2PInput local_input{};
    svanes::PeerId local_peer{};
    bool presentation_running = false;
    bool presentation_state_initialized = false;
};

class DoubleTimeP2PGame final : public svanes::IGame {
public:
    DoubleTimeP2PGame(std::uint16_t port,
                      std::optional<svanes::UdpAddress> entry = std::nullopt);
    void Initialize(svanes::GameContext &context) override;
    void Update(const svanes::FrameContext &frame) override;
    bool ShouldQuit() const override { return should_quit; }

private:
    std::uint16_t port;
    std::optional<svanes::UdpAddress> entry;
    std::unique_ptr<svanes::PeerGroup> network;
    std::unique_ptr<DoubleTimeP2PSimulation> simulation;
    std::unique_ptr<svanes::InputSync<DoubleTimeP2PInput,
                                      DoubleTimeP2PInputUse>> sync;
    bool should_quit = false;
    std::string last_status;
};
