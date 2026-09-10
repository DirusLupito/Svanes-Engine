#include "chris_game.hpp"

#include <svanes/audio/audio_manager.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <random>
#include <string>

constexpr std::int32_t kCharacterFrameWidth = 288;
constexpr std::int32_t kCharacterFrameHeight = 320;
constexpr float kCharacterFramesPerSecond = 12.0F;
constexpr float kCharacterSpawnLeft = 64.0F;
constexpr float kCharacterSpawnTop = 64.0F;

constexpr const char* kBackgroundMusicFilename = "audio/music/OldCity.wav";
constexpr const char* kBeatClickSoundFilename = "audio/sfx/jump.wav";

constexpr const char* kIdleSpriteSheetFilename = "sheet_umeko-idle.png";
constexpr std::int32_t kIdleFrameCount = 8;
constexpr float kBackgroundMusicBpm = 160.0F;
constexpr float kIdleSecondsPerBeat = 60.0F / kBackgroundMusicBpm;
constexpr float kIdleSecondsPerFrame = kIdleSecondsPerBeat / static_cast<float>(kIdleFrameCount);

// Fraction of the measured drift corrected on each beat sync check-in.
constexpr float kBeatDriftCorrectionRate = 0.1F;

// Below this, audio drift is left alone - not worth even a gentle speed nudge.
constexpr float kAudioDriftDeadZoneSeconds = 0.02F;
// At or above this, nudging speed would take too long to feel right; jump straight there.
constexpr float kAudioDriftSevereThresholdSeconds = 0.15F;
// How far off 1x speed to nudge music playback while correcting drift within the dead zone
// and the severe threshold. Small enough that the pitch/tempo shift isn't very noticeable.
constexpr float kAudioSpeedCorrectionOffset = 0.02F;

constexpr const char* kRunningSpriteSheetFilename = "sheet_umeko-run.png";
constexpr std::int32_t kRunningFrameCount = 10;
constexpr float kRunningDetectionThreshold = 0.01F;

constexpr svanes::Color kGroundColor{92, 64, 51, 255};
constexpr svanes::Color kPlatformColor{160, 82, 45, 255};

constexpr std::array<svanes::Color, 4> kBeatColorCycle{
    svanes::Color{17, 24, 39, 255},
    svanes::Color{127, 29, 29, 255},
    svanes::Color{20, 83, 45, 255},
    svanes::Color{30, 58, 138, 255},
};

constexpr std::int32_t kGroundZOrder = 1;
constexpr std::int32_t kSquareZOrder = 1;
constexpr std::int32_t kCharacterZOrder = 2;

constexpr float kInputSendIntervalSeconds = 1.0F / 60.0F;

ChrisGame::ChrisGame(std::string server_host)
    : network_state_socket(network_context, zmq::socket_type::sub)
    , network_input_socket(network_context, zmq::socket_type::push)
{
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<std::uint32_t> distribution;
    client_id = distribution(generator);

    network_state_socket.set(zmq::sockopt::subscribe, "");
    network_state_socket.connect(ChrisServerEndpoint(server_host, kChrisStatePort));
    network_input_socket.connect(ChrisServerEndpoint(server_host, kChrisInputPort));

    SDL_Log("Networking: client %u connecting to server at %s.", client_id, server_host.c_str());
}

void ChrisGame::SendInput(const svanes::FrameContext& frame)
{
    input_send_timer += frame.delta_seconds;
    if (input_send_timer < kInputSendIntervalSeconds) {
        return;
    }
    input_send_timer -= kInputSendIntervalSeconds;

    float horizontal_input = 0.0F;
    if (frame.input.IsDown(svanes::Key::Left)) {
        horizontal_input -= 1.0F;
    }
    if (frame.input.IsDown(svanes::Key::Right)) {
        horizontal_input += 1.0F;
    }

    float vertical_input = 0.0F;
    if (frame.input.IsDown(svanes::Key::Up)) {
        vertical_input -= 1.0F;
    }
    if (frame.input.IsDown(svanes::Key::Down)) {
        vertical_input += 1.0F;
    }

    const PlayerInputMessage input{client_id, horizontal_input, vertical_input, jump_requested_since_last_send};
    network_input_socket.send(zmq::buffer(&input, sizeof(input)), zmq::send_flags::dontwait);
    jump_requested_since_last_send = false;
}

void ChrisGame::PollServerState(const svanes::FrameContext& frame)
{
    std::optional<GameStateMessage> latest_state;
    std::optional<BeatSyncMessage> latest_beat_sync;

    while (true) {
        zmq::message_t message;
        const zmq::recv_result_t result = network_state_socket.recv(message, zmq::recv_flags::dontwait);
        if (!result.has_value()) {
            break;
        }

        if (message.size() == sizeof(GameStateMessage)) {
            latest_state = *message.data<GameStateMessage>();
        } else if (message.size() == sizeof(BeatSyncMessage)) {
            latest_beat_sync = *message.data<BeatSyncMessage>();
        } else {
            SDL_Log("Networking: received unexpected state packet of size %zu.", message.size());
        }
    }

    // Only the freshest beat sync matters: if several piled up because this client stalled
    // for a moment, reacting to the stale ones would compute their "now" well after the
    // server sent them, making up phantom drift that was really just queueing delay.
    if (latest_beat_sync.has_value()) {
        HandleBeatSync(*latest_beat_sync, frame.audio);
    }

    if (!latest_state.has_value()) {
        return;
    }

    svanes::Registry& world = frame.world;

    svanes::Transform& character = world.GetComponent<svanes::Transform>(character_entity);
    const bool should_run = std::abs(latest_state->character_x - character.x) > kRunningDetectionThreshold;
    character.x = latest_state->character_x;
    character.y = latest_state->character_y;

    if (should_run != is_running) {
        is_running = should_run;

        svanes::Sprite& sprite = world.GetComponent<svanes::Sprite>(character_entity);
        sprite.texture = is_running ? running_texture : idle_texture;

        svanes::SpriteAnimation& animation = world.GetComponent<svanes::SpriteAnimation>(character_entity);
        animation.frame_count = is_running ? kRunningFrameCount : kIdleFrameCount;
        animation.seconds_per_frame = is_running ? 1.0F / kCharacterFramesPerSecond : kIdleSecondsPerFrame;
        animation.current_frame = 0;
        animation.elapsed_seconds = 0.0F;
    }

    svanes::Transform& square = world.GetComponent<svanes::Transform>(square_entity);
    square.x = latest_state->shape_x;
    square.y = latest_state->shape_y;
}

void ChrisGame::HandleBeatSync(const BeatSyncMessage& message, svanes::AudioManager& audio)
{
    const auto candidate_start =
        std::chrono::steady_clock::now() - std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                std::chrono::duration<float>(message.elapsed_beat_seconds)
                                            );

    if (!local_beat_start.has_value()) {
        local_beat_start = candidate_start;
        audio.PlayMusic(background_music);
        SDL_Log("Beat sync: starting beat clock and audio playback.");
        return;
    }

    const float drift_seconds = std::chrono::duration<float>(candidate_start - *local_beat_start).count();
    SDL_Log("Beat sync: check-in, drift from local beat clock is %.4f seconds.", drift_seconds);

    // Nudge the local anchor a fraction of the way toward the server's rather than snapping
    // to it outright, so a correction never visibly skips or repeats a beat - it just makes
    // the beat boundaries a little shorter or longer than usual for the next few check-ins.
    *local_beat_start += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<float>(drift_seconds * kBeatDriftCorrectionRate)
    );
}

void ChrisGame::UpdateBeatColor(svanes::Registry& world, svanes::AudioManager& audio)
{
    if (!local_beat_start.has_value()) {
        return;
    }

    const float elapsed_seconds =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - *local_beat_start).count();
    const auto beat = static_cast<std::int64_t>(elapsed_seconds / kIdleSecondsPerBeat);

    if (beat == current_beat) {
        return;
    }
    current_beat = beat;

    const std::size_t color_index = static_cast<std::size_t>(current_beat) % kBeatColorCycle.size();
    world.GetComponent<svanes::SolidShape>(background_entity).color = kBeatColorCycle[color_index];
    audio.PlaySound(beat_click_sound);
}

void ChrisGame::UpdateAudioSync(svanes::AudioManager& audio)
{
    if (!local_beat_start.has_value()) {
        return;
    }

    // The music loops, but MusicPositionMilliseconds() reports position within the current
    // loop rather than cumulative time played, so the expected position needs wrapping to
    // the same range before the two are comparable.
    const float loop_duration_seconds = static_cast<float>(audio.MusicDurationMilliseconds()) / 1000.0F;
    if (loop_duration_seconds <= 0.0F) {
        return;
    }

    const float raw_expected_seconds =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - *local_beat_start).count();
    const float expected_seconds = std::fmod(raw_expected_seconds, loop_duration_seconds);
    const float actual_seconds = static_cast<float>(audio.MusicPositionMilliseconds()) / 1000.0F;

    // Wrap to the shortest signed distance around the loop, so being just before a loop
    // point and just after one don't look like they're a whole loop apart.
    float audio_drift_seconds = actual_seconds - expected_seconds;
    audio_drift_seconds = std::fmod(audio_drift_seconds + loop_duration_seconds * 1.5F, loop_duration_seconds) -
        loop_duration_seconds * 0.5F;

    // Ahead of where it should be: slow down to let the expected position catch up. Behind:
    // speed up. Small drift gets a gentle, barely audible speed nudge; anything past the
    // severe threshold jumps straight to the right spot instead of crawling there.
    if (std::abs(audio_drift_seconds) >= kAudioDriftSevereThresholdSeconds) {
        audio.SeekMusic(static_cast<std::int64_t>(expected_seconds * 1000.0F));
        audio.SetMusicPlaybackRate(1.0F);
        SDL_Log("Audio sync: seeking music to correct %.3f second drift.", audio_drift_seconds);
    } else if (audio_drift_seconds > kAudioDriftDeadZoneSeconds) {
        audio.SetMusicPlaybackRate(1.0F - kAudioSpeedCorrectionOffset);
    } else if (audio_drift_seconds < -kAudioDriftDeadZoneSeconds) {
        audio.SetMusicPlaybackRate(1.0F + kAudioSpeedCorrectionOffset);
    } else {
        audio.SetMusicPlaybackRate(1.0F);
    }
}

void ChrisGame::Initialize(svanes::GameContext& context)
{
    // The world is a fixed 1920x1080 design size shared with the server for collision.
    // Proportional mode scales/letterboxes that fixed world to fit whatever the actual
    // window size turns out to be, rather than assuming the window matches it exactly.
    context.camera.scale_mode = svanes::ScaleMode::Proportional;

    // Loaded now, but playback is deferred until the server's first beat sync message,
    // so audio and beat counting start together across every connected client.
    background_music = context.audio.LoadMusic(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kBackgroundMusicFilename);
    beat_click_sound = context.audio.LoadSound(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kBeatClickSoundFilename);

    background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background_entity,
        svanes::Transform{kChrisWorldWidth * 0.5F, kChrisWorldHeight * 0.5F, 0.0F}
    );
    context.world.AddComponent<svanes::SolidShape>(
        background_entity,
        svanes::SolidShape{
            svanes::Color{17, 24, 39, 255},
            svanes::Rectangle2D{0.0F, 0.0F, kChrisWorldWidth, kChrisWorldHeight},
        }
    );

    square_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        square_entity, svanes::Transform{kChrisWorldWidth * 0.5F, kChrisWorldHeight * 0.5F, 0.0F}
    );
    context.world.AddComponent<svanes::SolidShape>(
        square_entity,
        svanes::SolidShape{
            svanes::Color{37, 99, 235, 255},
            svanes::Rectangle2D{0.0F, 0.0F, kChrisShapeSize, kChrisShapeSize},
        }
    );
    context.world.AddComponent<svanes::ZOrder>(square_entity, svanes::ZOrder{kSquareZOrder});

    idle_texture = context.assets.LoadTexture(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kIdleSpriteSheetFilename);
    running_texture = context.assets.LoadTexture(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kRunningSpriteSheetFilename);

    character_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        character_entity,
        svanes::Transform{
            kCharacterSpawnLeft + kChrisCharacterWidth * 0.5F,
            kCharacterSpawnTop + kChrisCharacterHeight * 0.5F,
            0.0F,
        }
    );
    context.world.AddComponent<svanes::Sprite>(
        character_entity,
        svanes::Sprite{
            .texture = idle_texture,
            .geometry = svanes::Rectangle2D{0.0F, 0.0F, kChrisCharacterWidth, kChrisCharacterHeight},
        }
    );
    context.world.AddComponent<svanes::SpriteAnimation>(
        character_entity,
        svanes::SpriteAnimation{
            .frame_width = kCharacterFrameWidth,
            .frame_height = kCharacterFrameHeight,
            .frame_count = kIdleFrameCount,
            .seconds_per_frame = kIdleSecondsPerFrame,
        }
    );
    context.world.AddComponent<svanes::ZOrder>(character_entity, svanes::ZOrder{kCharacterZOrder});

    // Ground and platform are static level geometry, fixed in the same world space the
    // server collides against, so they never need to move after this.

    const float ground_top = kChrisWorldHeight - kChrisGroundHeight;

    ground_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        ground_entity, svanes::Transform{kChrisWorldWidth * 0.5F, ground_top + kChrisGroundHeight * 0.5F, 0.0F}
    );
    context.world.AddComponent<svanes::SolidShape>(
        ground_entity,
        svanes::SolidShape{kGroundColor, svanes::Rectangle2D{0.0F, 0.0F, kChrisWorldWidth, kChrisGroundHeight}}
    );
    context.world.AddComponent<svanes::ZOrder>(ground_entity, svanes::ZOrder{kGroundZOrder});

    const float platform_top = ground_top - kChrisPlatformTopHeightAboveGround;

    platform_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        platform_entity,
        svanes::Transform{
            kChrisPlatformLeft + kChrisPlatformWidth * 0.5F, platform_top + kChrisPlatformHeight * 0.5F, 0.0F
        }
    );
    context.world.AddComponent<svanes::SolidShape>(
        platform_entity,
        svanes::SolidShape{kPlatformColor, svanes::Rectangle2D{0.0F, 0.0F, kChrisPlatformWidth, kChrisPlatformHeight}}
    );
    context.world.AddComponent<svanes::ZOrder>(platform_entity, svanes::ZOrder{kGroundZOrder});
}

void ChrisGame::Update(const svanes::FrameContext& frame)
{
    if (frame.input.WasPressed(svanes::Key::Space)) {
        jump_requested_since_last_send = true;
    }

    SendInput(frame);
    PollServerState(frame);
    UpdateBeatColor(frame.world, frame.audio);
    UpdateAudioSync(frame.audio);

    // The world is a fixed size shared with the server, and Proportional scale mode
    // fits that fixed world into the window, so the camera stays centered on the world
    // itself - background, ground, and platform are all static and need no per-frame update.

    frame.camera.x = kChrisWorldWidth * 0.5F;
    frame.camera.y = kChrisWorldHeight * 0.5F;
}
