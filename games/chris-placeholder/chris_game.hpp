#pragma once

#include "network_config.hpp"

#include <svanes/audio/audio_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/game.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <zmq.hpp>

#include <string>

/**
 * Top level container for Chris's placeholder game.
 * Used to hold bridge components responsible for talking
 * to the engine.
 */
class ChrisGame final : public svanes::IGame {
public:
    /**
     * Constructs the game and connects it to the server's state broadcast socket.
     * @param server_state_address The ZeroMQ endpoint the server's state socket is bound to.
     */
    explicit ChrisGame(std::string server_state_address = kChrisServerStateConnectEndpoint);

    /**
     * Initializes the game with the provided context.
     * @param context The context for the game, providing access to the TextureManager.
     */
    void Initialize(svanes::GameContext& context) override;

    /**
     * Game specific update logic. Called by the engine once per frame.
     * @param frame The context for the current frame, providing access to the InputManager
     * and the time elapsed since the last frame.
     */
    void Update(const svanes::FrameContext& frame) override;

private:
    void ResolveCharacterHorizontal(svanes::Registry& world);
    void ResolveCharacterVertical(svanes::Registry& world);
    void PollServerState(svanes::Registry& world);


    // Total time elapsed since the start of the game, in seconds.
    float elapsed_seconds = 0.0F;
    svanes::Entity background_entity = 0;
    svanes::Entity square_entity = 0;
    svanes::Entity character_entity = 0;
    svanes::Entity ground_entity = 0;
    svanes::Entity platform_entity = 0;
    svanes::TextureHandle idle_texture{};
    svanes::TextureHandle running_texture{};
    svanes::SoundHandle jump_sound{};
    bool is_running = false;

    zmq::context_t network_context;
    zmq::socket_t network_state_socket;
};
