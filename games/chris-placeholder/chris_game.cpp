#include "chris_game.hpp"

#include <svanes/audio/audio_manager.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/input.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

constexpr float kSquareSize = 96.0F;

constexpr std::int32_t kCharacterFrameWidth = 288;
constexpr std::int32_t kCharacterFrameHeight = 320;
constexpr float kCharacterFramesPerSecond = 12.0F;
constexpr float kCharacterDisplayScale = 5.0F;
constexpr float kCharacterDisplayWidth = 36.0F * kCharacterDisplayScale;
constexpr float kCharacterDisplayHeight = 40.0F * kCharacterDisplayScale;
constexpr float kCharacterSpawnLeft = 64.0F;
constexpr float kCharacterSpawnTop = 64.0F;
constexpr float kCharacterMoveSpeed = 360.0F;

constexpr const char* kBackgroundMusicFilename = "audio/music/OldCity.wav";
constexpr const char* kJumpSoundFilename = "audio/sfx/jump.wav";

constexpr const char* kIdleSpriteSheetFilename = "sheet_umeko-idle.png";
constexpr std::int32_t kIdleFrameCount = 8;
constexpr float kBackgroundMusicBpm = 160.0F;
constexpr float kIdleSecondsPerBeat = 60.0F / kBackgroundMusicBpm;
constexpr float kIdleSecondsPerFrame = kIdleSecondsPerBeat / static_cast<float>(kIdleFrameCount);

constexpr const char* kRunningSpriteSheetFilename = "sheet_umeko-run.png";
constexpr std::int32_t kRunningFrameCount = 10;

constexpr float kGravityAcceleration = 1500.0F;
constexpr float kGroundHeight = 64.0F;
constexpr svanes::Color kGroundColor{92, 64, 51, 255};

constexpr float kJumpSpeed = 800.0F;

constexpr float kPlatformWidth = 300.0F;
constexpr float kPlatformHeight = 40.0F;
constexpr float kPlatformLeft = 500.0F;
constexpr float kPlatformTopHeightAboveGround = 260.0F;
constexpr svanes::Color kPlatformColor{160, 82, 45, 255};

constexpr std::int32_t kMaxResolutionIterations = 4;

constexpr std::int32_t kGroundZOrder = 1;
constexpr std::int32_t kSquareZOrder = 1;
constexpr std::int32_t kCharacterZOrder = 2;

void ChrisGame::Initialize(svanes::GameContext& context)
{
    const svanes::MusicHandle background_music =
        context.audio.LoadMusic(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kBackgroundMusicFilename);
    context.audio.PlayMusic(background_music);

    jump_sound = context.audio.LoadSound(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kJumpSoundFilename);

    const float output_width = static_cast<float>(context.output_width);
    const float output_height = static_cast<float>(context.output_height);

    background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background_entity,
        svanes::Transform{output_width * 0.5F, output_height * 0.5F, 0.0F}
    );
    context.world.AddComponent<svanes::SolidShape>(
        background_entity,
        svanes::SolidShape{
            svanes::Color{17, 24, 39, 255},
            svanes::Rectangle2D{0.0F, 0.0F, output_width, output_height},
        }
    );

    square_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(square_entity, svanes::Transform{});
    context.world.AddComponent<svanes::SolidShape>(
        square_entity,
        svanes::SolidShape{
            svanes::Color{37, 99, 235, 255},
            svanes::Rectangle2D{0.0F, 0.0F, kSquareSize, kSquareSize},
        }
    );
    context.world.AddComponent<svanes::Collider2D>(
        square_entity,
        svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kSquareSize, kSquareSize}}
    );
    context.world.AddComponent<svanes::ZOrder>(square_entity, svanes::ZOrder{kSquareZOrder});

    idle_texture = context.assets.LoadTexture(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kIdleSpriteSheetFilename);
    running_texture = context.assets.LoadTexture(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kRunningSpriteSheetFilename);

    character_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        character_entity,
        svanes::Transform{
            kCharacterSpawnLeft + kCharacterDisplayWidth * 0.5F,
            kCharacterSpawnTop + kCharacterDisplayHeight * 0.5F,
            0.0F,
        }
    );
    context.world.AddComponent<svanes::Sprite>(
        character_entity,
        svanes::Sprite{
            .texture = idle_texture,
            .geometry = svanes::Rectangle2D{0.0F, 0.0F, kCharacterDisplayWidth, kCharacterDisplayHeight},
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
    context.world.AddComponent<svanes::Kinematic2D>(
        character_entity,
        svanes::Kinematic2D{.acceleration_y = kGravityAcceleration}
    );
    context.world.AddComponent<svanes::Collider2D>(
        character_entity,
        svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kCharacterDisplayWidth, kCharacterDisplayHeight}}
    );
    context.world.AddComponent<svanes::ZOrder>(character_entity, svanes::ZOrder{kCharacterZOrder});

    ground_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(ground_entity, svanes::Transform{});
    context.world.AddComponent<svanes::SolidShape>(
        ground_entity,
        svanes::SolidShape{kGroundColor, svanes::Rectangle2D{}}
    );
    context.world.AddComponent<svanes::Collider2D>(ground_entity, svanes::Collider2D{svanes::Rectangle2D{}});
    context.world.AddComponent<svanes::ZOrder>(ground_entity, svanes::ZOrder{kGroundZOrder});

    platform_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(platform_entity, svanes::Transform{});
    context.world.AddComponent<svanes::SolidShape>(
        platform_entity,
        svanes::SolidShape{kPlatformColor, svanes::Rectangle2D{}}
    );
    context.world.AddComponent<svanes::Collider2D>(platform_entity, svanes::Collider2D{svanes::Rectangle2D{}});
    context.world.AddComponent<svanes::ZOrder>(platform_entity, svanes::ZOrder{kGroundZOrder});
}

void ChrisGame::ResolveCharacterHorizontal(svanes::Registry& world)
{
    svanes::Transform& character_transform = world.GetComponent<svanes::Transform>(character_entity);
    const svanes::Collider2D& character_collider = world.GetComponent<svanes::Collider2D>(character_entity);
    svanes::Kinematic2D& character_motion = world.GetComponent<svanes::Kinematic2D>(character_entity);

    for (std::int32_t iteration = 0; iteration < kMaxResolutionIterations; ++iteration) {
        std::optional<svanes::Collision2D> deepest_collision;

        world.ForEach<svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity candidate_entity, const svanes::Transform& candidate_transform, const svanes::Collider2D& candidate_collider) {
                if (candidate_entity == character_entity) {
                    return;
                }

                for (const svanes::Collision2D& collision : svanes::DetectCollisions(
                         character_collider.geometry, character_transform,
                         candidate_collider.geometry, candidate_transform
                     )) {
                    if (collision.normal.y != 0.0F) {
                        continue;
                    }

                    if (!deepest_collision.has_value() || collision.penetration_depth > deepest_collision->penetration_depth) {
                        deepest_collision = collision;
                    }
                }
            }
        );

        if (!deepest_collision.has_value()) {
            break;
        }

        character_transform.x += deepest_collision->normal.x * deepest_collision->penetration_depth;
        character_motion.velocity_x = 0.0F;
    }
}

void ChrisGame::ResolveCharacterVertical(svanes::Registry& world)
{
    svanes::Transform& character_transform = world.GetComponent<svanes::Transform>(character_entity);
    const svanes::Collider2D& character_collider = world.GetComponent<svanes::Collider2D>(character_entity);
    svanes::Kinematic2D& character_motion = world.GetComponent<svanes::Kinematic2D>(character_entity);

    for (std::int32_t iteration = 0; iteration < kMaxResolutionIterations; ++iteration) {
        std::optional<svanes::Collision2D> deepest_collision;

        world.ForEach<svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity candidate_entity, const svanes::Transform& candidate_transform, const svanes::Collider2D& candidate_collider) {
                if (candidate_entity == character_entity) {
                    return;
                }

                for (const svanes::Collision2D& collision : svanes::DetectCollisions(
                         character_collider.geometry, character_transform,
                         candidate_collider.geometry, candidate_transform
                     )) {
                    if (collision.normal.x != 0.0F) {
                        continue;
                    }

                    if (!deepest_collision.has_value() || collision.penetration_depth > deepest_collision->penetration_depth) {
                        deepest_collision = collision;
                    }
                }
            }
        );

        if (!deepest_collision.has_value()) {
            break;
        }

        character_transform.y += deepest_collision->normal.y * deepest_collision->penetration_depth;
        character_motion.velocity_y = 0.0F;
    }
}

void ChrisGame::Update(const svanes::FrameContext& frame)
{
    elapsed_seconds += frame.delta_seconds;

    const float output_width = static_cast<float>(frame.output_width);
    const float output_height = static_cast<float>(frame.output_height);

    svanes::Transform& background = frame.world.GetComponent<svanes::Transform>(background_entity);
    background.x = output_width * 0.5F;
    background.y = output_height * 0.5F;
    frame.world.GetComponent<svanes::SolidShape>(background_entity).geometry =
        svanes::Rectangle2D{0.0F, 0.0F, output_width, output_height};

    const svanes::Rectangle2D ground_rectangle{0.0F, 0.0F, output_width, kGroundHeight};
    const float ground_top = output_height - kGroundHeight;

    svanes::Transform& ground = frame.world.GetComponent<svanes::Transform>(ground_entity);
    ground.x = output_width * 0.5F;
    ground.y = ground_top + kGroundHeight * 0.5F;
    frame.world.GetComponent<svanes::SolidShape>(ground_entity).geometry = ground_rectangle;
    frame.world.GetComponent<svanes::Collider2D>(ground_entity).geometry = ground_rectangle;

    const svanes::Rectangle2D platform_rectangle{0.0F, 0.0F, kPlatformWidth, kPlatformHeight};
    const float platform_top = ground_top - kPlatformTopHeightAboveGround;

    svanes::Transform& platform = frame.world.GetComponent<svanes::Transform>(platform_entity);
    platform.x = kPlatformLeft + kPlatformWidth * 0.5F;
    platform.y = platform_top + kPlatformHeight * 0.5F;
    frame.world.GetComponent<svanes::SolidShape>(platform_entity).geometry = platform_rectangle;
    frame.world.GetComponent<svanes::Collider2D>(platform_entity).geometry = platform_rectangle;

    svanes::Transform& character = frame.world.GetComponent<svanes::Transform>(character_entity);
    character.y = std::clamp(character.y, kCharacterDisplayHeight * 0.5F, output_height - kCharacterDisplayHeight * 0.5F);

    ResolveCharacterVertical(frame.world);

    const float available_width = std::max(0.0F, output_width - kSquareSize);
    const float normalized_position = (std::sin(elapsed_seconds * 2.0F) + 1.0F) * 0.5F;

    svanes::Transform& square = frame.world.GetComponent<svanes::Transform>(square_entity);
    square.x = normalized_position * available_width + kSquareSize * 0.5F;
    square.y = output_height * 0.5F;

    float horizontal_input = 0.0F;
    if (frame.input.IsDown(svanes::Key::Left)) {
        horizontal_input -= 1.0F;
    }
    if (frame.input.IsDown(svanes::Key::Right)) {
        horizontal_input += 1.0F;
    }

    character.x += horizontal_input * kCharacterMoveSpeed * frame.delta_seconds;
    character.x = std::clamp(character.x, kCharacterDisplayWidth * 0.5F, output_width - kCharacterDisplayWidth * 0.5F);

    ResolveCharacterHorizontal(frame.world);

    svanes::Kinematic2D& character_motion = frame.world.GetComponent<svanes::Kinematic2D>(character_entity);
    const bool is_grounded = character_motion.velocity_y == 0.0F;
    if (is_grounded && frame.input.WasPressed(svanes::Key::Space)) {
        character_motion.velocity_y = -kJumpSpeed;
        frame.audio.PlaySound(jump_sound);
    }

    const bool should_run = horizontal_input != 0.0F;
    if (should_run != is_running) {
        is_running = should_run;

        svanes::Sprite& sprite = frame.world.GetComponent<svanes::Sprite>(character_entity);
        sprite.texture = is_running ? running_texture : idle_texture;

        svanes::SpriteAnimation& animation = frame.world.GetComponent<svanes::SpriteAnimation>(character_entity);
        animation.frame_count = is_running ? kRunningFrameCount : kIdleFrameCount;
        animation.seconds_per_frame = is_running ? 1.0F / kCharacterFramesPerSecond : kIdleSecondsPerFrame;
        animation.current_frame = 0;
        animation.elapsed_seconds = 0.0F;
    }
}
