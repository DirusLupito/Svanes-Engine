#include "chris_game.hpp"

#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
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
}

void ChrisGame::Update(const svanes::FrameContext& frame)
{
    elapsed_seconds += frame.delta_seconds;

    svanes::Transform& background = frame.world.GetComponent<svanes::Transform>(background_entity);
    background.width = static_cast<float>(frame.output_width);
    background.height = static_cast<float>(frame.output_height);

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
