/**
 * Where each of the six assigned tasks is implemented. Every entry is tagged inline
 * at the code it refers to, so searching this project for "TASK" finds all of them.
 *
 * - TASK 1, running the engine: main.cpp
 * - TASK 2A, static entity: the ground, in Initialize below
 * - TASK 2B, controllable entity: the goose, spawned in Initialize below and built
 *   in Goose::Spawn in goose.cpp
 * - TASK 2C, auto-moving entity: the enemy, spawned in Initialize below and driven
 *   along its path in Update below
 * - TASK 3, physics: world gravity in Initialize below, the goose's Gravity and
 *   Kinematic2D components in Goose::Spawn, and flight in Goose::Update
 * - TASK 4, controls: input read into a GooseIntent in Update below, and applied in
 *   Goose::Update in goose.cpp
 * - TASK 5, collision response: Goose::ResolveCollisions in goose.cpp
 * - TASK 6, scaling: the Tab key in Update below
 */

#include "erik_game.hpp"

#include "bullets.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <variant>
#include <vector>

namespace {

// the playable area, enclosed by the ground below and border walls on the other
// three sides
constexpr float kWorldLeft = 0.0F;
constexpr float kWorldRight = 6000.0F;
constexpr float kWorldTop = -1200.0F;
constexpr float kGroundTop = 974.0F;
constexpr float kGroundBottom = 1080.0F;
constexpr float kBorderThickness = 100.0F;

// how much larger than the view the sky is drawn, so its edges stay offscreen
constexpr float kSkyMargin = 1.5F;

// where in the window the goose is held, as a fraction of the output size, placing
// it centered horizontally and below center vertically to show more of what is ahead
constexpr float kCameraAnchorX = 0.5F;
constexpr float kCameraAnchorY = 0.68F;

// how long after tapping A or D a second tap still counts as a dash
constexpr float kDoubleTapWindow = 0.25F;

// bullets are destroyed once they leave this region, which is larger than the world
// so they expire out of sight rather than at the walls
constexpr svanes::Rectangle2D kBulletBounds{3000.0F, -60.0F, 6600.0F, 2800.0F};

constexpr float kEnemySize = 120.0F;
constexpr float kEnemyHealth = 10.0F;
constexpr float kBulletDamage = 1.0F;

// how far below itself the enemy aims, so it fires downward at the ground
constexpr float kEnemyAimDistance = 1000.0F;

// the enemy sweeps back and forth across kEnemyPathRadius either side of
// kEnemyPathCenterX, staying at a fixed height
constexpr float kEnemyPathCenterX = 1600.0F;
constexpr float kEnemyPathY = 300.0F;
constexpr float kEnemyPathRadius = 700.0F;
constexpr float kEnemyPathSpeed = 0.8F;

constexpr float kBulletKnockback = 350.0F;
constexpr float kContactKnockback = 550.0F;

/**
 * Creates one piece of immovable world geometry, drawn as a colored rectangle and
 * tagged Solid so the goose collides with it.
 *
 * @param world The registry the entity is created in.
 * @param center_x The world x position of the block's center.
 * @param center_y The world y position of the block's center.
 * @param width The block's width.
 * @param height The block's height.
 * @param color The color the block is drawn in.
 */
void CreateSolidBlock(
    svanes::Registry& world, float center_x, float center_y,
    float width, float height, svanes::Color color
)
{
    const svanes::Rectangle2D body{
        .width = width,
        .height = height,
    };

    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{
        .x = center_x,
        .y = center_y,
    });
    world.AddComponent<svanes::SolidShape>(entity, svanes::SolidShape{
        .color = color,
        .geometry = body,
    });
    world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});
    world.AddComponent<Solid>(entity);
}

}

void ErikGame::Initialize(svanes::GameContext& context)
{
    // the world gravity vector, applied by the engine every frame to any entity
    // holding both Kinematic2D and Gravity. Positive y is down
    context.gravity = {0.0F, 2000.0F};

    background = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(background, svanes::Transform{
        .x = 960.0F,
        .y = 540.0F,
    });
    context.world.AddComponent<svanes::SolidShape>(background, svanes::SolidShape{
        .color = svanes::Color{.blue = 255},
        .geometry = svanes::Rectangle2D{
            .width = 1920.0F,
            .height = 1080.0F,
        },
    });
    // drawn behind everything else, and resized to cover the view each frame in Update
    context.world.AddComponent<svanes::ZOrder>(background, svanes::ZOrder{-100});

    const float world_width = kWorldRight - kWorldLeft;
    const float world_center_x = (kWorldLeft + kWorldRight) * 0.5F;
    const float border_center_y = (kWorldTop + kGroundBottom) * 0.5F;
    const float border_height = kGroundBottom - kWorldTop;

    // TASK 2A, static entity: the ground. A Transform, a SolidShape to draw, a
    // Collider2D to be hit, and the Solid tag, with no motion components at all, so
    // nothing the engine does each frame can move it.
    CreateSolidBlock(
        context.world, world_center_x, (kGroundTop + kGroundBottom) * 0.5F,
        world_width, kGroundBottom - kGroundTop, svanes::Color{.red = 60, .green = 140, .blue = 70}
    );

    // the left, right and top walls, built the same way, closing off the arena
    CreateSolidBlock(
        context.world, kWorldLeft - kBorderThickness * 0.5F, border_center_y,
        kBorderThickness, border_height, svanes::Color{.red = 90, .green = 90, .blue = 110}
    );

    CreateSolidBlock(
        context.world, kWorldRight + kBorderThickness * 0.5F, border_center_y,
        kBorderThickness, border_height, svanes::Color{.red = 90, .green = 90, .blue = 110}
    );

    CreateSolidBlock(
        context.world, world_center_x, kWorldTop - kBorderThickness * 0.5F,
        world_width + 2.0F * kBorderThickness, kBorderThickness,
        svanes::Color{.red = 90, .green = 90, .blue = 110}
    );

    const svanes::TextureHandle orb_texture =
        context.assets.LoadTexture(std::string{ERIK_GAME_ASSETS_DIR} + "/darkworld_spawn_swirlingorb_idle.png");

    orb = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(orb, svanes::Transform{
        .x = 960.0F,
        .y = 540.0F,
    });
    context.world.AddComponent<svanes::Sprite>(orb, svanes::Sprite{
        .texture = orb_texture,
        .geometry = svanes::Rectangle2D{
            .width = 128.0F,
            .height = 128.0F,
        },
    });
    context.world.AddComponent<svanes::SpriteAnimation>(orb, svanes::SpriteAnimation{
        .frame_width = 128,
        .frame_height = 128,
        .frame_count = 4,
        .seconds_per_frame = 0.12F,
    });

    // TASK 2C, auto-moving entity: the enemy. It takes no input, and is walked
    // along its path by the game in Update.
    enemy.Spawn(context.world, {kEnemyPathCenterX, kEnemyPathY}, kEnemySize, kEnemyHealth);

    // TASK 2B, controllable entity: the goose. Built in Goose::Spawn, and driven
    // each frame by the player's input in Update.
    goose.Spawn(context, 400.0F, 700.0F);
}

void ErikGame::Update(const svanes::FrameContext& frame)
{
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        should_quit = true;
    }

    // TASK 6, scaling: Tab switches the camera between the two scale modes.
    // Constant draws everything at its literal pixel size, so resizing the window
    // reveals more of the world. Proportional rescales with the window, so
    // everything keeps the same fraction of the screen at any size.
    if (frame.input.WasPressed(svanes::Key::Tab)) {
        frame.camera.scale_mode = frame.camera.scale_mode == svanes::ScaleMode::Constant
            ? svanes::ScaleMode::Proportional
            : svanes::ScaleMode::Constant;
    }

    // TASK 4, controls: the raw keyboard and mouse state is read into a GooseIntent
    // here, and the goose acts on that instead of on the input device.
    GooseIntent intent{};
    if (frame.input.IsDown(svanes::Key::D)) {
        intent.move.x += 1.0F;
    }
    if (frame.input.IsDown(svanes::Key::A)) {
        intent.move.x -= 1.0F;
    }
    // a dash is a double tap of A or D. The first press starts that direction's
    // timer, and a second press while the timer is still running becomes the dash
    // instead
    left_tap_timer = std::max(left_tap_timer - frame.delta_seconds, 0.0F);
    right_tap_timer = std::max(right_tap_timer - frame.delta_seconds, 0.0F);

    if (frame.input.WasPressed(svanes::Key::A)) {
        if (left_tap_timer > 0.0F) {
            intent.dash = -1.0F;
            left_tap_timer = 0.0F;
        } else {
            left_tap_timer = kDoubleTapWindow;
        }
    }

    if (frame.input.WasPressed(svanes::Key::D)) {
        if (right_tap_timer > 0.0F) {
            intent.dash = 1.0F;
            right_tap_timer = 0.0F;
        } else {
            right_tap_timer = kDoubleTapWindow;
        }
    }

    intent.jump = frame.input.IsDown(svanes::Key::Space);
    intent.fire = frame.input.IsMouseButtonDown(svanes::MouseButton::Left);

    // the mouse arrives in screen coordinates, but the goose aims in world
    // coordinates, so the cursor is converted through the camera before being
    // handed over
    const svanes::Vector2D mouse = frame.input.MousePosition();
    const svanes::Rectangle2D aim = frame.camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});
    intent.aim_point = {aim.x, aim.y};

    goose.Update(frame, intent);

    // the camera follows by finding which world position currently sits at the
    // anchor point on screen, then shifting by however far the goose is from it.
    // Working through the anchor this way keeps the follow correct at any zoom,
    // scale mode or window size, since the camera itself does that conversion.
    const svanes::Transform& goose_transform = frame.world.GetComponent<svanes::Transform>(goose.GetEntity());
    const svanes::Rectangle2D anchor = frame.camera.ScreenToWorld(
        {frame.output_width * kCameraAnchorX, frame.output_height * kCameraAnchorY, 0.0F, 0.0F}
    );

    frame.camera.x += goose_transform.x - anchor.x;
    frame.camera.y += goose_transform.y - anchor.y;

    const svanes::Rectangle2D view = frame.camera.ScreenToWorld({
        frame.output_width * 0.5F, frame.output_height * 0.5F,
        static_cast<float>(frame.output_width), static_cast<float>(frame.output_height)
    });

    svanes::Transform& sky = frame.world.GetComponent<svanes::Transform>(background);
    sky.x = view.x;
    sky.y = view.y;

    svanes::Rectangle2D& sky_body = std::get<svanes::Rectangle2D>(
        frame.world.GetComponent<svanes::SolidShape>(background).geometry
    );
    sky_body.width = view.width * kSkyMargin;
    sky_body.height = view.height * kSkyMargin;

    elapsed_seconds += frame.delta_seconds;

    // TASK 2C, auto-moving entity: the enemy's motion for this frame. Its position
    // is a sine wave over elapsed time, sweeping it back and forth above the arena
    // while it fires downward
    if (enemy.IsAlive()) {
        const svanes::Vector2D enemy_position = enemy.Position(frame.world);

        EnemyIntent enemy_intent{};
        enemy_intent.move_to = {
            kEnemyPathCenterX + std::sin(elapsed_seconds * kEnemyPathSpeed) * kEnemyPathRadius,
            kEnemyPathY,
        };
        enemy_intent.fire = true;
        enemy_intent.aim_point = {enemy_position.x, enemy_position.y + kEnemyAimDistance};

        enemy.Update(frame, enemy_intent);
    }

    // UpdateBullets moves and destroys the bullets and reports what they struck,
    // leaving the game to decide what a hit means
    for (const BulletHit& hit : UpdateBullets(frame.world, kBulletBounds)) {
        if (hit.target == goose.GetEntity()) {
            goose.ApplyKnockback(frame.world, hit.direction, kBulletKnockback);
        } else if (enemy.IsAlive() && hit.target == enemy.GetEntity()) {
            enemy.ApplyDamage(frame.world, kBulletDamage);
        }
    }

    if (enemy.IsAlive()) {
        const std::vector<svanes::Collision2D> contact = svanes::DetectCollisions(
            frame.world.GetComponent<svanes::Collider2D>(goose.GetEntity()).geometry,
            frame.world.GetComponent<svanes::Transform>(goose.GetEntity()),
            frame.world.GetComponent<svanes::Collider2D>(enemy.GetEntity()).geometry,
            frame.world.GetComponent<svanes::Transform>(enemy.GetEntity())
        );

        if (!contact.empty()) {
            goose.ApplyKnockback(frame.world, contact[0].normal, kContactKnockback);
        }
    }
}

bool ErikGame::ShouldQuit() const
{
    return should_quit;
}
