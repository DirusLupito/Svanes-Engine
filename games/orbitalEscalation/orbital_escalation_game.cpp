#include "orbital_escalation_game.hpp"
#include <svanes/MenuUtilities/text_label.hpp>

#include <svanes/attractor_system.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/collision_pass.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/input.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <random>
#include <thread>

constexpr std::int32_t kSquarePixels = 300;
constexpr float kPlanetRadius = 4200.0F;
const svanes::TicCount kCollisionFlashLifetime = svanes::SecondsToTics(0.5);
const svanes::TicCount kCollisionFlashExpansionTime =
    svanes::SecondsToTics(0.05);
constexpr float kCollisionFlashInitialRadius = 4000.0F;
constexpr float kCollisionFlashExpansionRadius = 8000.0F;
const svanes::TicCount kPauseFlashPeriod = svanes::SecondsToTics(1.0);
constexpr std::uint8_t kPauseLabelMinimumAlpha = 64;

/**
 * Helper function to create an entity with a Timeline component that is a
 * child of the provided gameplay_timeline entity. This is useful for creating
 * entities that should be synchronized with the main gameplay timeline.
 *
 * @param world The registry in which to create the entity.
 * @param gameplay_timeline The parent timeline entity to which the new entity's
 * Timeline will be linked.
 *
 * @return The newly created entity with a Timeline component.
 */
static svanes::Entity CreateTimedEntity(svanes::Registry &world,
                                        svanes::Entity gameplay_timeline) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Timeline>(entity, gameplay_timeline);
    return entity;
}

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
 * Updates all active collision flashes, expanding them at the start of their
 * lifetime and fading them out before destroying them when their lifetime has
 * elapsed.
 *
 * @param world The registry containing the collision flash entities.
 * @param collision_flashes The list of active collision flash entities.
 */
static void
UpdateCollisionFlashes(svanes::Registry &world,
                       std::vector<svanes::Entity> &collision_flashes) {
    // if the flash has been alive for longer than its lifetime,
    // destroy it and remove it from the list of active flashes.
    std::erase_if(collision_flashes, [&](svanes::Entity flash) {
        const svanes::TicCount elapsed_tics =
            world.GetComponent<svanes::Timeline>(flash).GetTotalTics();
        if (elapsed_tics >= kCollisionFlashLifetime) {
            world.DestroyEntity(flash);
            return true;
        }

        // Well we're already iterating over all the flashes,
        // so we might as well update their size and alpha here too.

        const float expansion =
            std::min(static_cast<float>(elapsed_tics) /
                         static_cast<float>(kCollisionFlashExpansionTime),
                     1.0F);
        const float fade =
            elapsed_tics <= kCollisionFlashExpansionTime
                ? 1.0F
                : 1.0F - static_cast<float>(elapsed_tics -
                                            kCollisionFlashExpansionTime) /
                             static_cast<float>(kCollisionFlashLifetime -
                                                kCollisionFlashExpansionTime);
        auto &gradient = world.GetComponent<svanes::RadialGradient2D>(flash);
        gradient.geometry.radius =
            std::lerp(kCollisionFlashInitialRadius,
                      kCollisionFlashExpansionRadius, expansion);
        gradient.center_color.alpha = static_cast<std::uint8_t>(255.0F * fade);
        return false;
    });
}

/**
 * Creates a collision flash at the specified point and adds it to the list of
 * active flashes.
 *
 * Just a rapidly expanding ball that's much brighter at its center than at its
 * edge. A ball that will add white to the screen where it is drawn, at greater
 * intensity at its center than at its edge. Will fade out much slower than it
 * expands. That is how we get the effect of an explosion of light.
 * Its Timeline supplies the elapsed lifetime; there is no second accumulator.
 *
 * @param world The registry to create the collision flash entity in.
 * @param gameplay_timeline The parent timeline entity to which the new flash's
 * Timeline will be linked.
 * @param collision_flashes The list to which the new collision flash will be
 * added.
 * @param contact_point The world-space point at which the collision flash will
 * be drawn.
 */
static void CreateCollisionFlash(svanes::Registry &world,
                                 svanes::Entity gameplay_timeline,
                                 std::vector<svanes::Entity> &collision_flashes,
                                 svanes::Vector2D contact_point) {

    const svanes::Entity flash = CreateTimedEntity(world, gameplay_timeline);
    world.AddComponent<svanes::Transform>(
        flash, svanes::Transform{contact_point.x, contact_point.y});

    // could probably have also worked with alpha blending and an
    // all white radial gradient.
    world.AddComponent<svanes::RadialGradient2D>(
        flash, svanes::RadialGradient2D{
                   .geometry = svanes::Circle2D{0.0F, 0.0F,
                                                kCollisionFlashInitialRadius},
                   .center_color = svanes::Color{255, 255, 255, 255},
                   .edge_color = svanes::Color{0, 0, 0, 0},
                   .blend_mode = svanes::BlendMode::Additive,
               });

    collision_flashes.push_back(flash);
}

/**
 * Creates a flash for each circle involved in the supplied collisions, except
 * when either entity is the planet. The flash is placed on the circle's side
 * facing the shape it collided with.
 *
 * @param world The registry containing the colliding entities.
 * @param gameplay_timeline The parent timeline entity to which the new flash's
 * Timelines will be linked.
 * @param a The first entity in the collision pair.
 * @param b The second entity in the collision pair.
 * @param collisions The collisions detected between the two entities.
 * @param collision_flashes The list to which newly created flashes will be
 * added.
 */
static void CreateCollisionFlashes(
    svanes::Registry &world, svanes::Entity gameplay_timeline, svanes::Entity a,
    svanes::Entity b, const std::vector<svanes::Collision2D> &collisions,
    std::vector<svanes::Entity> &collision_flashes) {
    // only the planet has a PointAttractor2D component, so if either entity has
    // one, the planet is involved in the collision and we don't want to create
    // flashes for it.
    if (world.HasComponent<svanes::PointAttractor2D>(a) ||
        world.HasComponent<svanes::PointAttractor2D>(b)) {
        return;
    }

    // we only want to create flashes for circles because
    // its really easy to figure out where to put the flash for a circle
    // (just put it on the edge of the circle in the direction of the collision
    // normal)

    const auto *circle_a = std::get_if<svanes::Circle2D>(
        &world.GetComponent<svanes::Collider2D>(a).geometry);

    const auto *circle_b = std::get_if<svanes::Circle2D>(
        &world.GetComponent<svanes::Collider2D>(b).geometry);

    if (circle_a == nullptr && circle_b == nullptr) {
        return;
    }

    const svanes::Transform &transform_a =
        world.GetComponent<svanes::Transform>(a);
    const svanes::Transform &transform_b =
        world.GetComponent<svanes::Transform>(b);

    for (const svanes::Collision2D &collision : collisions) {
        if (circle_a != nullptr) {
            const svanes::Vector2D center{
                transform_a.x + circle_a->x,
                transform_a.y + circle_a->y,
            };

            CreateCollisionFlash(world, gameplay_timeline, collision_flashes,
                                 center - collision.normal * circle_a->radius);
        }
        if (circle_b != nullptr) {
            const svanes::Vector2D center{
                transform_b.x + circle_b->x,
                transform_b.y + circle_b->y,
            };

            CreateCollisionFlash(world, gameplay_timeline, collision_flashes,
                                 center + collision.normal * circle_b->radius);
        }
    }
}

/**
 * Applies an acceleration to two entities based on their collision, if they
 * have collided to slam them apart. The acceleration is applied in the
 * direction of the collision normal.
 *
 * @param world The registry containing the entities.
 * @param gameplay_timeline The parent timeline entity to which any new
 * entities' Timelines will be linked.
 * @param a The first entity.
 * @param b The second entity.
 * @param collision_flashes The list to which flashes created by the collisions
 * will be added.
 */
static void ApplyCollisionAcceleration(
    svanes::Registry &world, svanes::Entity gameplay_timeline, svanes::Entity a,
    svanes::Entity b, const std::vector<svanes::Collision2D> &collisions,
    std::vector<svanes::Entity> &collision_flashes) {
    for (const svanes::Collision2D &collision : collisions) {
        const bool a_is_planet =
            world.HasComponent<svanes::PointAttractor2D>(a);
        const bool b_is_planet =
            world.HasComponent<svanes::PointAttractor2D>(b);
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

    CreateCollisionFlashes(world, gameplay_timeline, a, b, collisions,
                           collision_flashes);
}

/**
 * Applies collision acceleration to all pairs of entities in the provided list.
 *
 * @param world The registry containing the entities.
 * @param gameplay_timeline The parent timeline entity to which any new
 * entities' Timelines will be linked.
 * @param entities The list of entities to check for collisions and apply
 * acceleration.
 * @param collision_flashes The list to which flashes created by the collisions
 * will be added.
 * @param driver The driver used for broad and narrow phase collision work.
 */
static void
ApplyCollisionAcceleration(svanes::Registry &world,
                           svanes::Entity gameplay_timeline,
                           const std::vector<svanes::Entity> &entities,
                           std::vector<svanes::Entity> &collision_flashes,
                           svanes::AsyncParallelForDriver &driver) {
    auto collisions = svanes::DetectEntityCollisions(world, entities, driver);

    // For the sake of deterministic collision response across architectures
    // and the internet, we want all collisions to be processed in the same
    // order. So we sort the pairs of colliding entities. It doesn't really
    // matter how we sort them, so long as the order is consistent across
    // platforms. In this case, we assume that entity IDs are consistent across
    // platforms. If this is incorrect, collision will not necessarily produce
    // the same results given the same inputs on two different platforms.

    std::sort(collisions.begin(), collisions.end(),
              [](const svanes::EntityCollision2D &a,
                 const svanes::EntityCollision2D &b) {
                  return a.a < b.a || (a.a == b.a && a.b < b.b);
              });

    for (const svanes::EntityCollision2D &pair : collisions) {
        ApplyCollisionAcceleration(world, gameplay_timeline, pair.a, pair.b,
                                   pair.collisions, collision_flashes);
    }
}

/**
 * Creates a gradient image of size kSquarePixels x kSquarePixels, where the
 * color transitions from a light color in the top-left corner to a dark color
 * in the bottom-right corner
 *
 * @return An ImageData object containing the generated gradient image.
 */
svanes::ImageData CreateGradientImage() {
    svanes::ImageData image{
        .width = kSquarePixels,
        .height = kSquarePixels,
        .rgba_pixels = std::vector<std::uint8_t>(
            static_cast<std::size_t>(kSquarePixels) * kSquarePixels * 4),
    };

    constexpr float start_r = 0xF0;
    constexpr float start_g = 0xF0;
    constexpr float start_b = 0xF0;
    constexpr float end_r = 0x00;
    constexpr float end_g = 0x00;
    constexpr float end_b = 0xFE;

    // We just linearly interpolate the color from the top-left corner to
    // the bottom-right corner of the square.

    for (std::int32_t y = 0; y < kSquarePixels; ++y) {
        for (std::int32_t x = 0; x < kSquarePixels; ++x) {
            // Measure distance with the 1-norm.
            // Then we normalize it to the range [0, 1]
            // so it can be used as a lerp parameter.
            const float t =
                static_cast<float>(x + y) / (2.0F * (kSquarePixels - 1));
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kSquarePixels + x) * 4;
            image.rgba_pixels[offset + 0] =
                static_cast<std::uint8_t>(std::lerp(start_r, end_r, t));
            image.rgba_pixels[offset + 1] =
                static_cast<std::uint8_t>(std::lerp(start_g, end_g, t));
            image.rgba_pixels[offset + 2] =
                static_cast<std::uint8_t>(std::lerp(start_b, end_b, t));
            image.rgba_pixels[offset + 3] = 0xFF;
        }
    }

    return image;
}

/**
 * Creates a desert planet layer with a given radius, color, and z-order in the
 * provided registry.
 *
 * @param world The registry to create the planet layer in.
 * @param radius The radius of the planet layer.
 * @param color The color of the planet layer.
 * @param z_order The z-order of the planet layer for rendering.
 *
 * @return The entity representing the created planet layer.
 */
static svanes::Entity CreatePlanetLayer(svanes::Registry &world, float radius,
                                        svanes::Color color,
                                        std::int32_t z_order) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity);
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{color, svanes::Circle2D{0.0F, 0.0F, radius}});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    return entity;
}

void OrbitalEscalationGame::CreateNonPlayerNonPlanetEntities(
    svanes::Registry &world) {
    std::random_device rd;
    std::mt19937 gen(rd());

    // Uniformly distribute the NPC entities at any radian angle around the
    // planet.

    std::uniform_real_distribution<float> angle_dist(
        0.0F, 2.0F * static_cast<float>(std::numbers::pi));

    // Uniformly distribute the NPC entities at any distance from the planet's
    // surface, between minimum_distance_of_npc_entities_from_planet and
    // maximum_distance_of_npc_entities_from_planet, measured from the surface
    // of the planet to the center of the NPC entity.
    std::uniform_real_distribution<float> distance_dist(
        minimum_distance_of_npc_entities_from_planet,
        maximum_distance_of_npc_entities_from_planet);

    // Uniformly distribute the magnitude of the initial velocity of the NPC
    // entities, as given by the magnitude of the tangent to the vector from the
    // planet to the NPC entity when it is first spawned, between
    // minimum_magnitude_of_npc_entity_initial_velocity and
    // maximum_magnitude_of_npc_entity_initial_velocity.
    std::uniform_real_distribution<float> velocity_dist(
        minimum_magnitude_of_npc_entity_initial_velocity,
        maximum_magnitude_of_npc_entity_initial_velocity);

    // Uniformly distribute the angular velocity of the NPC entities,
    // between minimum_angular_velocity_of_npc_entities and
    // maximum_angular_velocity_of_npc_entities, measured in radians per second.
    std::uniform_real_distribution<float> angular_velocity_dist(
        minimum_angular_velocity_of_npc_entities,
        maximum_angular_velocity_of_npc_entities);

    for (uint32_t i = 0; i < num_npc_entities_to_spawn; ++i) {
        const float angle = angle_dist(gen);
        const float distance = distance_dist(gen);
        const float velocity_magnitude =
            svanes::PerSecondToPerTic(velocity_dist(gen));
        const float angular_velocity =
            svanes::PerSecondToPerTic(angular_velocity_dist(gen));

        // Calculate the position of the entity based on the angle and distance
        // from the planet's surface.
        const float x = std::cos(angle) * (kPlanetRadius + distance);
        const float y = std::sin(angle) * (kPlanetRadius + distance);

        svanes::Entity entity =
            CreateTimedEntity(world, gameplay_timeline_entity);
        world.AddComponent<svanes::Transform>(entity, svanes::Transform{x, y});
        world.AddComponent<svanes::Kinematic2D>(entity);

        // Set the initial velocity tangent to the vector from the planet to the
        // entity

        // Every other entity will have a clockwise initial velocity, while the
        // others will have a counter-clockwise initial velocity.
        if (i % 2 == 0) {
            world.GetComponent<svanes::Kinematic2D>(entity).velocity_x =
                -std::sin(angle) * velocity_magnitude;
            world.GetComponent<svanes::Kinematic2D>(entity).velocity_y =
                std::cos(angle) * velocity_magnitude;
        } else {
            world.GetComponent<svanes::Kinematic2D>(entity).velocity_x =
                std::sin(angle) * velocity_magnitude;
            world.GetComponent<svanes::Kinematic2D>(entity).velocity_y =
                -std::cos(angle) * velocity_magnitude;
        }

        world.GetComponent<svanes::Kinematic2D>(entity).angular_velocity =
            angular_velocity;

        // Determine the color uniformly along all three channels, with the
        // alpha channel being fully opaque.
        std::uniform_int_distribution<std::uint16_t> color_dist(0, 255);
        const svanes::Color color{static_cast<std::uint8_t>(color_dist(gen)),
                                  static_cast<std::uint8_t>(color_dist(gen)),
                                  static_cast<std::uint8_t>(color_dist(gen)),
                                  255};

        // Cycle through shapes: box, circle, triangle
        if (i % 3 == 0) {
            svanes::Rectangle2D rectangle_geometry{0.0F, 0.0F, 100.0F, 100.0F};
            world.AddComponent<svanes::Collider2D>(
                entity, svanes::Collider2D{rectangle_geometry});
            world.AddComponent<svanes::SolidShape>(
                entity, svanes::SolidShape{color, rectangle_geometry});
        } else if (i % 3 == 1) {
            svanes::Circle2D circle_geometry{0.0F, 0.0F, 100.0F};
            world.AddComponent<svanes::Collider2D>(
                entity, svanes::Collider2D{circle_geometry});
            world.AddComponent<svanes::SolidShape>(
                entity, svanes::SolidShape{color, circle_geometry});
        } else {
            svanes::Triangle2D triangle_geometry{
                .vertices = {svanes::Vector2D{0.0F, 100.0F},
                             svanes::Vector2D{-50.0F, -50.0F},
                             svanes::Vector2D{50.0F, -50.0F}}};
            world.AddComponent<svanes::Collider2D>(
                entity, svanes::Collider2D{triangle_geometry});
            world.AddComponent<svanes::SolidShape>(
                entity, svanes::SolidShape{color, triangle_geometry});
        }
        non_planet_non_player_entities.push_back(entity);
    }
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
                "games/orbitalEscalation/assets/fonts/consola.ttf", 24.0F),
            .alignment = svanes::TextAlignment::TopCenter,
            .visible = false,
        });

    // number of logical cores
    // this number includes the main thread, which is also treated as a worker
    // thread, so we don't need to add 1 to it.
    context.concurrency = // 1;
        static_cast<std::uint32_t>(std::thread::hardware_concurrency());
    const svanes::TextureHandle gradient_texture =
        context.assets.CreateTexture(CreateGradientImage());
    constexpr float square_size = static_cast<float>(kSquarePixels);
    const svanes::Rectangle2D square_geometry{0.0F, 0.0F, square_size,
                                              square_size};
    const svanes::Transform player_start{0.0F, -kPlanetRadius - 800.0F};
    context.camera.zoom = 0.02F;
    const svanes::Rectangle2D anchor = context.camera.ScreenToWorld(
        {context.camera.OutputWidth() * 0.5F,
         context.camera.OutputHeight() * 0.25F, 0.0F, 0.0F});
    context.camera.x += player_start.x - anchor.x;
    context.camera.y += player_start.y - anchor.y;
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


    square_entity = CreateTimedEntity(context.world, gameplay_timeline_entity);
    context.world.AddComponent<svanes::Collider2D>(
        square_entity, svanes::Collider2D{square_geometry});
    context.world.AddComponent<svanes::Kinematic2D>(square_entity);
    context.world.GetComponent<svanes::Kinematic2D>(square_entity).velocity_x =
        svanes::PerSecondToPerTic(3000.0F);
    context.world.AddComponent<svanes::Transform>(square_entity, player_start);
    context.world.AddComponent<svanes::Sprite>(
        square_entity, svanes::Sprite{.texture = gradient_texture,
                                      .geometry = square_geometry});
    context.world.AddComponent<svanes::RadialGradient2D>(
        square_entity,
        svanes::RadialGradient2D{
            .geometry = svanes::Circle2D{0.0F, 0.0F, square_size * 5.0F},
            .center_color = svanes::Color{255, 255, 255, 160},
            .edge_color = svanes::Color{64, 128, 255, 0},
        });

    planet_entity = CreatePlanetLayer(context.world, kPlanetRadius,
                                      {255, 127, 38, 255}, -3);
    CreatePlanetLayer(context.world, 3900.0F, {185, 122, 87, 255}, -2);
    const svanes::Entity inner_layer =
        CreatePlanetLayer(context.world, 3750.0F, {127, 127, 127, 255}, -1);
    context.world.AddComponent<svanes::Collider2D>(
        planet_entity,
        svanes::Collider2D{svanes::Circle2D{0.0F, 0.0F, kPlanetRadius}});
    context.world.AddComponent<svanes::PointAttractor2D>(
        planet_entity,
        svanes::PointAttractor2D{.accelerationField = AttractionField,
                                 .cutoff_radius = std::nullopt});

    for (const svanes::Rectangle2D wall :
         {svanes::Rectangle2D{-200000.0F, 0.0F, 100000.0F, 500000.0F},
          svanes::Rectangle2D{200000.0F, 0.0F, 100000.0F, 500000.0F},
          svanes::Rectangle2D{0.0F, -200000.0F, 500000.0F, 100000.0F},
          svanes::Rectangle2D{0.0F, 200000.0F, 500000.0F, 100000.0F}}) {
        const svanes::Entity entity = context.world.CreateEntity();
        const svanes::Rectangle2D geometry{0.0F, 0.0F, wall.width, wall.height};
        context.world.AddComponent<svanes::Transform>(
            entity, svanes::Transform{wall.x, wall.y});
        context.world.AddComponent<svanes::Collider2D>(
            entity, svanes::Collider2D{geometry});
        context.world.AddComponent<svanes::SolidShape>(
            entity,
            svanes::SolidShape{svanes::Color{255, 0, 0, 255}, geometry});
        boundary_entities.push_back(entity);
    }

    CreateNonPlayerNonPlanetEntities(context.world);

    collidable_entities.push_back(square_entity);
    collidable_entities.push_back(planet_entity);
    collidable_entities.insert(collidable_entities.end(),
                               non_planet_non_player_entities.begin(),
                               non_planet_non_player_entities.end());
}

void OrbitalEscalationGame::Update(const svanes::FrameContext &frame) {
    UpdateCollisionFlashes(frame.world, collision_flashes);

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


    // constexpr float camera_speed = 300.0F;
    // frame.camera.x += camera_speed * frame.delta_seconds * (
    //     frame.input.IsDown(svanes::Key::Right) -
    //     frame.input.IsDown(svanes::Key::Left)
    // );
    // frame.camera.y += camera_speed * frame.delta_seconds * (
    //     frame.input.IsDown(svanes::Key::Down) -
    //     frame.input.IsDown(svanes::Key::Up)
    // );

    // Camera follows the player, centered on the screen.
    if (frame.world.HasComponent<svanes::Transform>(square_entity)) {
        const svanes::Transform &player =
            frame.world.GetComponent<svanes::Transform>(square_entity);
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

    if (physics.world.HasComponent<svanes::Kinematic2D>(square_entity)) {
        svanes::Kinematic2D &motion =
            physics.world.GetComponent<svanes::Kinematic2D>(square_entity);
        motion.acceleration_x = static_cast<float>(
            svanes::PerSecondSquaredToPerTicSquared(1000.0F) *
            (physics.input.IsDown(svanes::Key::D) -
             physics.input.IsDown(svanes::Key::A)));
        motion.acceleration_y = static_cast<float>(
            svanes::PerSecondSquaredToPerTicSquared(1000.0F) *
            (physics.input.IsDown(svanes::Key::S) -
             physics.input.IsDown(svanes::Key::W)));
        motion.angular_acceleration =
            svanes::PerSecondSquaredToPerTicSquared(100.0F) *
            (physics.input.IsDown(svanes::Key::E) -
             physics.input.IsDown(svanes::Key::Q));
    }

    // Reset the acceleration of all non-player, non-planet entities to zero
    // before applying collision acceleration.
    for (svanes::Entity entity : non_planet_non_player_entities) {
        if (physics.world.HasComponent<svanes::Kinematic2D>(entity)) {
            auto &motion =
                physics.world.GetComponent<svanes::Kinematic2D>(entity);
            motion.acceleration_x = 0.0F;
            motion.acceleration_y = 0.0F;
            motion.angular_acceleration = 0.0F;
        }
    }

    // ApplyCollisionAcceleration(frame.world, {square_entity, planet_entity});
    std::vector<svanes::Entity> destroyed_entities;
    physics.world.ForEach<svanes::Transform, svanes::Collider2D>(
        [&](svanes::Entity entity, const svanes::Transform &transform,
            const svanes::Collider2D &collider) {
            if (std::ranges::find(boundary_entities, entity) !=
                boundary_entities.end()) {
                return;
            }
            for (svanes::Entity boundary : boundary_entities) {
                const auto collisions = svanes::DetectCollisions(
                    collider.geometry, transform,
                    physics.world.GetComponent<svanes::Collider2D>(boundary)
                        .geometry,
                    physics.world.GetComponent<svanes::Transform>(boundary));
                if (!collisions.empty()) {
                    CreateCollisionFlashes(
                        physics.world, gameplay_timeline_entity, entity,
                        boundary, collisions, collision_flashes);
                    destroyed_entities.push_back(entity);
                    break;
                }
            }
        });
    for (svanes::Entity entity : destroyed_entities) {
        physics.world.DestroyEntity(entity);
        std::erase(collidable_entities, entity);
        std::erase(non_planet_non_player_entities, entity);
    }

    ApplyCollisionAcceleration(physics.world, gameplay_timeline_entity,
                               collidable_entities, collision_flashes,
                               physics.parallel_for);
}

bool OrbitalEscalationGame::ShouldQuit() const { return should_quit; }
