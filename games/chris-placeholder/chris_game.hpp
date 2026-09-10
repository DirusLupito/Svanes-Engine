#pragma once

#include "network_protocol.hpp"

#include <svanes/audio/audio_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/game.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <zmq.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

/**
 * Top level container for Chris's placeholder game.
 * Used to hold bridge components responsible for talking
 * to the engine.
 *
 * Position for both the character and the shape entity is authoritative on
 * the server: this client only ever sends its own directional input and
 * renders whatever position the server last broadcast. That keeps this same
 * class usable regardless of which entity the server currently has this
 * client controlling.
 */
class ChrisGame final : public svanes::IGame {
public:
    /**
     * Constructs the game, generates a random client id, and connects to the server.
     * @param server_host The hostname or IP address the server is running on.
     */
    explicit ChrisGame(std::string server_host = kChrisDefaultServerHost);

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
    void SendInput(const svanes::FrameContext& frame);
    void PollServerState(const svanes::FrameContext& frame);
    void HandleBeatSync(const BeatSyncMessage& message, svanes::AudioManager& audio);
    void UpdateBeatColor(svanes::Registry& world, svanes::AudioManager& audio);
    void UpdateAudioSync(svanes::AudioManager& audio);

    svanes::Entity background_entity = 0;
    svanes::Entity square_entity = 0;
    svanes::Entity character_entity = 0;
    svanes::Entity ground_entity = 0;
    svanes::Entity platform_entity = 0;
    svanes::TextureHandle idle_texture{};
    svanes::TextureHandle running_texture{};
    svanes::MusicHandle background_music{};
    // Played on every beat flip as an audible check that the flip is landing exactly on the
    // music's beat, not just close to it. Not meant to survive into the finished game.
    svanes::SoundHandle beat_click_sound{};
    bool is_running = false;

    std::uint32_t client_id;
    zmq::context_t network_context;
    zmq::socket_t network_state_socket;
    zmq::socket_t network_input_socket;
    float input_send_timer = 0.0F;
    bool jump_requested_since_last_send = false;

    // Anchors this client's beat clock to the server's, set on the first BeatSyncMessage
    // received and nudged toward the server's clock on every message after that. Music
    // playback is, in turn, corrected to track this clock in UpdateAudioSync.
    std::optional<std::chrono::steady_clock::time_point> local_beat_start;
    std::int64_t current_beat = -1;
};
