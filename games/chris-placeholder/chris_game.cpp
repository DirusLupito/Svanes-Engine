#include "chris_game.hpp"

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
constexpr float kCharacterDisplayX = 64.0F;
constexpr float kCharacterDisplayY = 64.0F;
constexpr float kCharacterMoveSpeed = 360.0F;

constexpr const char* kIdleSpriteSheetFilename = "sheet_umeko-idle.png";
constexpr std::int32_t kIdleFrameCount = 8;

constexpr const char* kRunningSpriteSheetFilename = "sheet_umeko-run.png";
constexpr std::int32_t kRunningFrameCount = 10;

constexpr float kGravityAcceleration = 1500.0F;
constexpr float kGroundHeight = 64.0F;
constexpr svanes::Color kGroundColor{92, 64, 51, 255};

constexpr float kJumpSpeed = 800.0F;

constexpr float kPlatformWidth = 300.0F;
constexpr float kPlatformHeight = 40.0F;
constexpr float kPlatformOffsetX = 500.0F;
constexpr float kPlatformTopHeightAboveGround = 260.0F;
constexpr svanes::Color kPlatformColor{160, 82, 45, 255};

constexpr std::int32_t kMaxResolutionIterations = 4;

svanes::TextureHandle CreateSolidColorTexture(svanes::TextureManager& assets, svanes::Color color)
{
    const svanes::ImageData image{
        .width = 1,
        .height = 1,
        .rgba_pixels = {color.red, color.green, color.blue, color.alpha},
    };

    return assets.CreateTexture(image);
}

void ChrisGame::Initialize(svanes::GameContext& context)
{
    background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(background_entity);
    context.world.AddComponent<svanes::SolidRectangle>(
        background_entity,
        svanes::SolidRectangle{svanes::Color{17, 24, 39, 255}}
    );

    const svanes::TextureHandle square_texture =
        CreateSolidColorTexture(context.assets, svanes::Color{37, 99, 235, 255});

    square_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        square_entity,
        svanes::Transform{0.0F, 0.0F, kSquareSize, kSquareSize}
    );
    context.world.AddComponent<svanes::Sprite>(
        square_entity,
        svanes::Sprite{.texture = square_texture}
    );
    context.world.AddComponent<svanes::Collider2D>(square_entity, svanes::Collider2D{});

    idle_texture = context.assets.LoadTexture(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kIdleSpriteSheetFilename);
    running_texture = context.assets.LoadTexture(std::string{CHRIS_GAME_ASSETS_DIR} + "/" + kRunningSpriteSheetFilename);

    character_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        character_entity,
        svanes::Transform{kCharacterDisplayX, kCharacterDisplayY, kCharacterDisplayWidth, kCharacterDisplayHeight}
    );
    context.world.AddComponent<svanes::Sprite>(
        character_entity,
        svanes::Sprite{.texture = idle_texture}
    );
    context.world.AddComponent<svanes::SpriteAnimation>(
        character_entity,
        svanes::SpriteAnimation{
            .frame_width = kCharacterFrameWidth,
            .frame_height = kCharacterFrameHeight,
            .frame_count = kIdleFrameCount,
            .seconds_per_frame = 1.0F / kCharacterFramesPerSecond,
        }
    );
    context.world.AddComponent<svanes::Kinematic2D>(
        character_entity,
        svanes::Kinematic2D{.acceleration_y = kGravityAcceleration}
    );
    context.world.AddComponent<svanes::Collider2D>(character_entity, svanes::Collider2D{});

    const svanes::TextureHandle ground_texture = CreateSolidColorTexture(context.assets, kGroundColor);

    ground_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(ground_entity);
    context.world.AddComponent<svanes::Sprite>(
        ground_entity,
        svanes::Sprite{.texture = ground_texture}
    );
    context.world.AddComponent<svanes::Collider2D>(ground_entity, svanes::Collider2D{});

    const svanes::TextureHandle platform_texture = CreateSolidColorTexture(context.assets, kPlatformColor);

    platform_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(platform_entity);
    context.world.AddComponent<svanes::Sprite>(
        platform_entity,
        svanes::Sprite{.texture = platform_texture}
    );
    context.world.AddComponent<svanes::Collider2D>(platform_entity, svanes::Collider2D{});
}

void ChrisGame::ResolveCharacterHorizontal(svanes::Registry& world)
{
    svanes::Transform& character = world.GetComponent<svanes::Transform>(character_entity);
    svanes::Kinematic2D& character_motion = world.GetComponent<svanes::Kinematic2D>(character_entity);

    for (std::int32_t iteration = 0; iteration < kMaxResolutionIterations; ++iteration) {
        const svanes::Transform* deepest_static_transform = nullptr;
        float deepest_overlap_width = 0.0F;

        world.ForEach<svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity candidate_entity, const svanes::Transform& static_transform, const svanes::Collider2D&) {
                if (candidate_entity == character_entity) {
                    return;
                }

                const std::optional<svanes::Rectangle> overlap = svanes::GetOverlap(character, static_transform);
                if (overlap.has_value() && overlap->width > deepest_overlap_width) {
                    deepest_overlap_width = overlap->width;
                    deepest_static_transform = &static_transform;
                }
            }
        );

        if (deepest_static_transform == nullptr) {
            break;
        }

        if (character.x < deepest_static_transform->x) {
            character.x = deepest_static_transform->x - character.width;
        } else {
            character.x = deepest_static_transform->x + deepest_static_transform->width;
        }

        character_motion.velocity_x = 0.0F;
    }
}

void ChrisGame::ResolveCharacterVertical(svanes::Registry& world)
{
    svanes::Transform& character = world.GetComponent<svanes::Transform>(character_entity);
    svanes::Kinematic2D& character_motion = world.GetComponent<svanes::Kinematic2D>(character_entity);

    for (std::int32_t iteration = 0; iteration < kMaxResolutionIterations; ++iteration) {
        const svanes::Transform* deepest_static_transform = nullptr;
        float deepest_overlap_height = 0.0F;

        world.ForEach<svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity candidate_entity, const svanes::Transform& static_transform, const svanes::Collider2D&) {
                if (candidate_entity == character_entity) {
                    return;
                }

                const std::optional<svanes::Rectangle> overlap = svanes::GetOverlap(character, static_transform);
                if (overlap.has_value() && overlap->height > deepest_overlap_height) {
                    deepest_overlap_height = overlap->height;
                    deepest_static_transform = &static_transform;
                }
            }
        );

        if (deepest_static_transform == nullptr) {
            break;
        }

        if (character_motion.velocity_y >= 0.0F) {
            character.y = deepest_static_transform->y - character.height;
        } else {
            character.y = deepest_static_transform->y + deepest_static_transform->height;
        }

        character_motion.velocity_y = 0.0F;
    }
}

void ChrisGame::Update(const svanes::FrameContext& frame)
{
    elapsed_seconds += frame.delta_seconds;

    svanes::Transform& background = frame.world.GetComponent<svanes::Transform>(background_entity);
    background.width = static_cast<float>(frame.output_width);
    background.height = static_cast<float>(frame.output_height);

    svanes::Transform& ground = frame.world.GetComponent<svanes::Transform>(ground_entity);
    ground.x = 0.0F;
    ground.y = static_cast<float>(frame.output_height) - kGroundHeight;
    ground.width = static_cast<float>(frame.output_width);
    ground.height = kGroundHeight;

    svanes::Transform& platform = frame.world.GetComponent<svanes::Transform>(platform_entity);
    platform.x = kPlatformOffsetX;
    platform.y = ground.y - kPlatformTopHeightAboveGround;
    platform.width = kPlatformWidth;
    platform.height = kPlatformHeight;

    ResolveCharacterVertical(frame.world);

    const float available_width = std::max(0.0F, static_cast<float>(frame.output_width) - kSquareSize);
    const float normalized_position = (std::sin(elapsed_seconds * 2.0F) + 1.0F) * 0.5F;

    svanes::Transform& square = frame.world.GetComponent<svanes::Transform>(square_entity);
    square.x = normalized_position * available_width;
    square.y = (static_cast<float>(frame.output_height) - kSquareSize) * 0.5F;

    float horizontal_input = 0.0F;
    if (frame.input.IsDown(svanes::Key::Left)) {
        horizontal_input -= 1.0F;
    }
    if (frame.input.IsDown(svanes::Key::Right)) {
        horizontal_input += 1.0F;
    }

    svanes::Transform& character = frame.world.GetComponent<svanes::Transform>(character_entity);
    character.x += horizontal_input * kCharacterMoveSpeed * frame.delta_seconds;
    character.x = std::clamp(character.x, 0.0F, static_cast<float>(frame.output_width) - character.width);

    ResolveCharacterHorizontal(frame.world);

    svanes::Kinematic2D& character_motion = frame.world.GetComponent<svanes::Kinematic2D>(character_entity);
    const bool is_grounded = character_motion.velocity_y == 0.0F;
    if (is_grounded && frame.input.WasPressed(svanes::Key::Space)) {
        character_motion.velocity_y = -kJumpSpeed;
    }

    const bool should_run = horizontal_input != 0.0F;
    if (should_run != is_running) {
        is_running = should_run;

        svanes::Sprite& sprite = frame.world.GetComponent<svanes::Sprite>(character_entity);
        sprite.texture = is_running ? running_texture : idle_texture;

        svanes::SpriteAnimation& animation = frame.world.GetComponent<svanes::SpriteAnimation>(character_entity);
        animation.frame_count = is_running ? kRunningFrameCount : kIdleFrameCount;
        animation.current_frame = 0;
        animation.elapsed_seconds = 0.0F;
    }
}
