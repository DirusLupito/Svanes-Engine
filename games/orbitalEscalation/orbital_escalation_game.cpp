#include "orbital_escalation_game.hpp"
#include "attachment_system.hpp"
#include "combat/damage_system.hpp"
#include "controls.hpp"
#include "serialization/ship_serialization.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/input.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>

constexpr float kPlanetRadius = 4200.0F;
constexpr float kAttachmentLaunchSpeed = 1500.0F;
constexpr float kPlayerHealth = 100.0F;
constexpr float kPlanetImpactDamage = 25.0F;
const svanes::TicCount kPauseFlashPeriod = svanes::SecondsToTics(1.0);
constexpr std::uint8_t kPauseLabelMinimumAlpha = 64;

/**
 * Helper for the planet's gravitational field.
 * Returns the acceleration vector at a given offset from the planet's center.
 *
 * @param offset_to_source The offset vector from the planet's center to the
 * point of interest.
 * @return The acceleration vector at the given offset, pointing towards the
 * planet's center.
 */
static svanes::Vector2D AttractionField(svanes::Vector2D offset_to_source) {
    const float distance = std::hypot(offset_to_source.x, offset_to_source.y);
    if (distance == 0.0F) {
        return {};
    }
    const float strength =
        svanes::PerSecondSquaredToPerTicSquared(180000000.0F) /
        (1.0F + distance * distance / kPlanetRadius);
    return offset_to_source / distance * strength;
}

/**
 * Creates a desert planet layer with a given radius, color, and z-order.
 *
 * @param radius The radius of the planet layer.
 * @param color The color of the planet layer.
 * @param z_order The z-order of the planet layer for rendering.
 *
 * @return The visual representing the planet layer.
 */
static Visual CreatePlanetLayer(float radius, svanes::Color color,
                                std::int32_t z_order) {
    return {svanes::SolidShape{color, svanes::Circle2D{0.0F, 0.0F, radius}},
            z_order};
}

/**
 * Applies an acceleration to two entities based on their collision, if they
 * have collided to slam them apart. The acceleration is applied in the
 * direction of the collision normal.
 *
 * @param world The registry containing the entities.
 * @param a The first entity.
 * @param b The second entity.
 */
static bool ApplyCollisionAcceleration(svanes::Registry &world,
                                       svanes::Entity a, svanes::Entity b) {
    const auto collisions = svanes::DetectCollisions(
        world.GetComponent<svanes::Collider2D>(a).geometry,
        world.GetComponent<svanes::Transform>(a),
        world.GetComponent<svanes::Collider2D>(b).geometry,
        world.GetComponent<svanes::Transform>(b));
    for (const svanes::Collision2D &collision : collisions) {
        const svanes::Vector2D acceleration =
            collision.normal *
            svanes::PerSecondSquaredToPerTicSquared(400000.0F);
        if (world.HasComponent<svanes::Kinematic2D>(a)) {
            auto &motion = world.GetComponent<svanes::Kinematic2D>(a);
            motion.acceleration_x += acceleration.x;
            motion.acceleration_y += acceleration.y;
        }
        if (world.HasComponent<svanes::Kinematic2D>(b)) {
            auto &motion = world.GetComponent<svanes::Kinematic2D>(b);
            motion.acceleration_x -= acceleration.x;
            motion.acceleration_y -= acceleration.y;
        }
    }
    return !collisions.empty();
}

void OrbitalEscalationGame::Initialize(svanes::GameContext &context) {
    // Create the overarching gameplay timeline entity, which we can use
    // to pause all gameplay, or speedup/slowdown all gameplay.
    gameplay_timeline_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Timeline>(gameplay_timeline_entity);

    pause_timeline_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Timeline>(pause_timeline_entity);

    pause_label_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::TextLabel>(
        pause_label_entity,
        svanes::TextLabel{
            .text = "PAUSED",
            .position = {context.camera.Viewport().width * 0.5F, 16.0F},
            .color = {255, 0, 0, 255},
            .font = context.fonts.LoadFont(
                svanes::AssetPath("assets/orbitalEscalation/fonts/consola.ttf"),
                24.0F),
            .alignment = svanes::TextAlignment::TopCenter,
            .visible = false,
        });

    planet.emplace(
        context.world,
        PlanetDefinition{
            .attractor = {.accelerationField = AttractionField,
                          .cutoff_radius = std::nullopt,
                          .allow_parallel = true},
            .collider = {svanes::Circle2D{0.0F, 0.0F, kPlanetRadius}},
            .visuals = {CreatePlanetLayer(kPlanetRadius, {255, 127, 38, 255},
                                          -3),
                        CreatePlanetLayer(3900.0F, {185, 122, 87, 255}, -2),
                        CreatePlanetLayer(3750.0F, {127, 127, 127, 255}, -1)},
        });
    planet->GetTransform(context.world) = {0.0F, 0.0F};
    planet->UpdateVisuals(context.world);

    player_ship.emplace(context.world, gameplay_timeline_entity,
                        LoadShip(svanes::AssetPath(
                            "assets/orbitalEscalation/ships/player.json")));
    context.world.AddComponent<Health>(player_ship->GetEntity(),
                                       Health{kPlayerHealth});
    const svanes::Transform player_start{0.0F, -kPlanetRadius - 800.0F};
    player_ship->GetTransform(context.world) = player_start;
    player_ship->GetKinematic(context.world).velocity_x =
        svanes::PerSecondToPerTic(300.0F);
    UpdateAttachments(context.world);
    player_ship->UpdateVisuals(context.world);
    context.camera.zoom = 0.02F;
    context.camera.x = player_start.x;
    context.camera.y = player_start.y;
    const svanes::Rectangle2D view =
        context.camera.ScreenToWorld(context.camera.Viewport());

    background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background_entity, svanes::Transform{view.x, view.y});
    context.world.AddComponent<svanes::SolidShape>(
        background_entity,
        svanes::SolidShape{
            svanes::Color{0, 0, 68, 255},
            svanes::Rectangle2D{0.0F, 0.0F, view.width, view.height}});
    context.world.AddComponent<svanes::ZOrder>(background_entity,
                                               svanes::ZOrder{-100});
}

void OrbitalEscalationGame::Update(const svanes::FrameContext &frame) {
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        should_quit = true;
    }

    // Pauses the simulation
    if (frame.input.WasPressed(svanes::Key::P)) {
        auto &timeline = frame.world.GetComponent<svanes::Timeline>(
            gameplay_timeline_entity);
        if (timeline.IsPaused()) {
            timeline.Unpause();
        } else {
            timeline.Pause();
        }
    }

    if (frame.input.WasPressed(svanes::Key::Tab)) {
        frame.camera.scale_mode =
            frame.camera.scale_mode == svanes::ScaleMode::Constant
                ? svanes::ScaleMode::Proportional
                : svanes::ScaleMode::Constant;
    }

    auto &pause_label =
        frame.world.GetComponent<svanes::TextLabel>(pause_label_entity);

    pause_label.visible =
        frame.world.GetComponent<svanes::Timeline>(gameplay_timeline_entity)
            .IsPaused();

    // I use a cosine wave to make the pause label flash while the game is
    // paused.
    const svanes::TicCount pause_tics =
        frame.world.GetComponent<svanes::Timeline>(pause_timeline_entity)
            .GetTotalTics() %
        kPauseFlashPeriod;

    const float pause_flash_phase = 2.0F * std::numbers::pi_v<float> *
                                    static_cast<float>(pause_tics) /
                                    static_cast<float>(kPauseFlashPeriod);

    const float pause_flash_amount =
        0.5F * (1.0F + std::cos(pause_flash_phase));

    pause_label.color.alpha = static_cast<std::uint8_t>(
        std::lerp(static_cast<float>(kPauseLabelMinimumAlpha), 255.0F,
                  pause_flash_amount));

    // Adjust for any changes in the camera, especially regarding proportional
    // scaling.
    pause_label.position.x = frame.camera.Viewport().width * 0.5F;

    // 1.1^delta
    // Rolling harder on the mouse wheel will zoom in and out
    // faster compared to rolling the same distance slowly.
    const float zoom = std::clamp(
        frame.camera.zoom * std::pow(1.1F, frame.input.MouseWheelThisFrame().y),
        0.01F, 100.0F);
    frame.camera.zoom = zoom;

    UpdateAttachments(frame.world);

    // Right click fires all attached missiles.
    if (player_ship &&
        !frame.world.GetComponent<svanes::Timeline>(gameplay_timeline_entity)
             .IsPaused() &&
        frame.input.WasMouseButtonPressed(svanes::MouseButton::Right)) {
        const auto &pose = player_ship->GetTransform(frame.world);

        const auto forward =
            frame.world.GetComponent<Propulsion>(player_ship->GetEntity())
                .GetForward();

        const float cosine = std::cos(pose.rotation);
        const float sine = std::sin(pose.rotation);

        const svanes::Vector2D direction{forward.x * cosine - forward.y * sine,
                                         forward.x * sine + forward.y * cosine};

        auto released = player_ship->DetachAttachments(
            frame.world,
            direction * svanes::PerSecondToPerTic(kAttachmentLaunchSpeed));

        for (auto &attachment : released) {
            detached_attachments.push_back(std::move(attachment));
        }
    }

    // Visuals are entities that just have one single visual component, like a
    // sprite or a solid shape. So even though the engine will update the ship
    // entity and the planet entity, we need to update their visuals separately
    // to make sure they are drawn correctly on the screen.

    if (player_ship) {
        player_ship->UpdateVisuals(frame.world);
    }

    for (const auto &attachment : detached_attachments) {
        attachment.UpdateVisuals(frame.world);
    }

    planet->UpdateVisuals(frame.world);

    // Camera follows the player, centered on the screen.
    if (player_ship) {
        const svanes::Transform &player =
            frame.world.GetComponent<svanes::Transform>(
                player_ship->GetEntity());
        frame.camera.x = player.x;
        frame.camera.y = player.y;
    }

    const svanes::Rectangle2D view =
        frame.camera.ScreenToWorld(frame.camera.Viewport());
    svanes::Transform &background =
        frame.world.GetComponent<svanes::Transform>(background_entity);
    background.x = view.x;
    background.y = view.y;
    svanes::Rectangle2D &background_rectangle = std::get<svanes::Rectangle2D>(
        frame.world.GetComponent<svanes::SolidShape>(background_entity)
            .geometry);
    background_rectangle.width = view.width;
    background_rectangle.height = view.height;
}

void OrbitalEscalationGame::PhysicsUpdate(
    const svanes::PhysicsContext &physics) {

    // No physics update should occur if the gameplay timeline is paused.
    if (physics.world.GetComponent<svanes::Timeline>(gameplay_timeline_entity)
            .IsPaused()) {
        return;
    }

    if (player_ship) {
        physics.world.GetComponent<PropulsionControl>(
            player_ship->GetEntity()) = ReadShipControls(physics.input);
    }
    ApplyPropulsion(physics.world);

    if (!player_ship) {
        return;
    }
    const bool touching_planet = ApplyCollisionAcceleration(
        physics.world, player_ship->GetEntity(), planet->GetEntity());
    if (touching_planet && !player_touching_planet &&
        ApplyDamage(physics.world, player_ship->GetEntity(),
                    kPlanetImpactDamage)) {
        player_ship.reset();
    }
    player_touching_planet = touching_planet;
}

bool OrbitalEscalationGame::ShouldQuit() const { return should_quit; }
