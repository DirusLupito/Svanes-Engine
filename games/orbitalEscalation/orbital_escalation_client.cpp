#include "orbital_escalation_client.hpp"
#include "combat/damage_system.hpp"
#include "combat/missile_system.hpp"
#include "combat/weapon_system.hpp"
#include "controls.hpp"
#include "effects/collision_flashes.hpp"
#include "game_objects/deletion_boxes.hpp"
#include "game_objects/world_obstacles.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/network/clientserver/network_client.hpp>
#include <svanes/physics_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <unordered_set>
#include <utility>

const svanes::TicCount kPauseFlashPeriod = svanes::SecondsToTics(1.0);
constexpr std::uint8_t kPauseLabelMinimumAlpha = 64;
constexpr svanes::TicCount kClientFrameTics = 5000; // 1/200 second
constexpr float kPlanetImpactDamage = 25.0F;
constexpr float kCollisionAcceleration = 400000.0F;

/**
 * Pushes colliding objects apart using the contact normals.
 *
 * @param world The registry containing the colliding objects.
 * @param pair The objects and their collision contacts.
 */
static void ApplyCollisionAcceleration(svanes::Registry &world,
                                       const svanes::EntityCollision2D &pair) {
    for (const auto &collision : pair.collisions) {
        const auto acceleration =
            collision.normal *
            svanes::PerSecondSquaredToPerTicSquared(kCollisionAcceleration);
        if (world.HasComponent<svanes::Kinematic2D>(pair.a)) {
            auto &motion = world.GetComponent<svanes::Kinematic2D>(pair.a);
            motion.acceleration_x += acceleration.x;
            motion.acceleration_y += acceleration.y;
        }

        if (world.HasComponent<svanes::Kinematic2D>(pair.b)) {
            auto &motion = world.GetComponent<svanes::Kinematic2D>(pair.b);
            motion.acceleration_x -= acceleration.x;
            motion.acceleration_y -= acceleration.y;
        }
    }
}

OrbitalEscalationClient::OrbitalEscalationClient(std::string server_address)
    : server_address(std::move(server_address)),
      assets(svanes::AssetPath("assets/orbitalEscalation")) {
    if (this->server_address.empty()) {
        throw std::invalid_argument("The server address must be provided.");
    }
}

void OrbitalEscalationClient::Initialize(svanes::ClientContext &context) {
    context.network.Connect(server_address);
    std::cout << "Joining " << server_address << '.' << std::endl;

    gameplay_timeline.emplace(context.world);
    context.world.AddComponent<svanes::Timeline>(gameplay_timeline->Get());

    effects_timeline.emplace(context.world);
    context.world.AddComponent<svanes::Timeline>(effects_timeline->Get());

    deletion_boxes = CreateDeletionBoxes(context.world);
    repelling_triangle.emplace(CreateRepellingTriangle(context.world));
    rotating_square.emplace(
        CreateRotatingSquare(context.world, gameplay_timeline->Get()));

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

    context.camera.zoom = 0.02F;

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

svanes::NetworkMessage OrbitalEscalationClient::CreateJoinRequest() {
    return {};
}

void OrbitalEscalationClient::HandleJoinResult(
    const svanes::JoinResult &result) {
    switch (result.status) {
    case svanes::JoinStatus::Accepted:
        client_id = result.client_id;
        std::cout << "Connected as player " << result.client_id << '.'
                  << std::endl;
        break;
    case svanes::JoinStatus::Rejected:
        std::cout << "Server rejected the join request.\n";
        break;
    case svanes::JoinStatus::TimedOut:
        std::cout << "Server join request timed out.\n";
        break;
    }
}

void OrbitalEscalationClient::HandleDisconnect() {
    std::cout << "Disconnected from the server.\n";
    should_quit = true;
}

svanes::TicCount OrbitalEscalationClient::GetFrameIntervalTics() const {
    const auto interval =
        svanes::RationalNumber{kClientFrameTics} /
        (svanes::RationalNumber{1} / client_loop_timeline.GetTicSize());

    return interval.GetWholePart() +
           (interval.GetFractionalPart().GetNumerator() != 0);
}

void OrbitalEscalationClient::Update(const svanes::ClientFrameContext &frame) {
    if (should_quit) {
        return;
    }

    // The world update messages received for this frame.
    const auto messages = frame.network.PollMessages();

    // Each update describes the complete world state, so only apply the last
    // one before the engine renders this frame.
    if (!messages.empty()) {
        ApplyWorldUpdate(frame, WorldUpdate::Read(messages.back()));
    }

    if (frame.input.WasPressed(svanes::Key::Escape)) {
        frame.network.Disconnect();
        should_quit = true;
        return;
    }

    // +/- will speed up or slow down the client loop.
    const bool faster = frame.input.WasPressed(svanes::Key::Equals) ||
                        frame.input.WasPressed(svanes::Key::KeypadPlus);
    const bool slower = frame.input.WasPressed(svanes::Key::Minus) ||
                        frame.input.WasPressed(svanes::Key::KeypadMinus);
    if (faster != slower) {
        client_loop_timeline.SetTicSize(client_loop_timeline.GetTicSize() /
                                        (faster
                                             ? svanes::RationalNumber{2}
                                             : svanes::RationalNumber{1, 2}));
        std::cout << "Client loop rate: "
                  << static_cast<double>(svanes::TicsPerSecond) /
                         GetFrameIntervalTics()
                  << " updates/second requested.\n";
    }

    // The pause label's animation follows local time.
    pause_timeline.Advance(frame.real_delta_tics);
    if (frame.input.WasPressed(svanes::Key::Tab)) {
        frame.camera.scale_mode =
            frame.camera.scale_mode == svanes::ScaleMode::Constant
                ? svanes::ScaleMode::Proportional
                : svanes::ScaleMode::Constant;
    }

    auto &pause_label =
        frame.world.GetComponent<svanes::TextLabel>(pause_label_entity);

    // I use a cosine wave to make the pause label flash while the game is
    // paused.
    const svanes::TicCount pause_tics =
        pause_timeline.GetTotalTics() % kPauseFlashPeriod;

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

    // Forward the player's input to the server.
    SendInput(frame);

    // Local effects follow frame time while gameplay waits for a complete
    // physics step or takes its state from a server update.
    auto &timeline =
        frame.world.GetComponent<svanes::Timeline>(gameplay_timeline->Get());

    const bool paused = timeline.IsPaused();
    timeline.Pause();
    svanes::AdvanceTimelines(frame.world, frame.real_delta_tics);
    svanes::AdvanceSpriteAnimations(frame.world);
    if (!paused) {
        timeline.Unpause();
    }

    // As on the server, advance one complete step when due, retain the
    // fractional interval, and discard overdue whole steps. A server update
    // or pause discards time accumulated for the previous predicted state.
    if (messages.empty() && planet && !paused) {
        pending_physics_tics += frame.real_delta_tics;
        if (pending_physics_tics >= svanes::DefaultPhysicsStepTics) {
            auto &effects = frame.world.GetComponent<svanes::Timeline>(
                effects_timeline->Get());
            const bool effects_paused = effects.IsPaused();

            // Effects already advanced for this frame and must not also
            // advance with the gameplay step.
            effects.Pause();
            svanes::AdvanceTimelines(frame.world,
                                     svanes::DefaultPhysicsStepTics);
            Predict(frame.world, frame.events);
            svanes::AdvanceSpriteAnimations(frame.world);
            if (!effects_paused) {
                effects.Unpause();
            }
            pending_physics_tics %= svanes::DefaultPhysicsStepTics;
        }
    } else {
        pending_physics_tics = 0;
    }

    // Update the visuals of all purely client effects.

    UpdateCollisionFlashes(frame.world, collision_flashes);

    // Follow the ship after either prediction or an authoritative update.
    if (player_ship) {
        const auto &pose = objects.at(*player_ship)->GetTransform(frame.world);
        frame.camera.x = pose.x;
        frame.camera.y = pose.y;
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

void OrbitalEscalationClient::ApplyWorldUpdate(
    const svanes::ClientFrameContext &frame, const WorldUpdate &update) {
    // The registry containing the client's displayed objects and local UI.
    auto &world = frame.world;

    // The master clock shared by the displayed gameplay objects.
    auto &timeline =
        world.GetComponent<svanes::Timeline>(gameplay_timeline->Get());

    // Replace predicted gameplay time with the server's clock state.
    timeline.SetProgress(update.total_tics, 0, svanes::RationalNumber{0});
    timeline.SetTicSize(update.tic_size);
    if (update.paused) {
        timeline.Pause();
        world.GetComponent<svanes::Timeline>(effects_timeline->Get()).Pause();
    } else {
        timeline.Unpause();
        world.GetComponent<svanes::Timeline>(effects_timeline->Get()).Unpause();
    }

    world.GetComponent<svanes::TextLabel>(pause_label_entity).visible =
        update.paused;

    // We create the planet once, then update its displayed position and
    // rotation given subsequent world updates.
    if (!planet) {
        planet.emplace(world, Planet::MakeDefinition());
    }

    planet->GetTransform(world) = update.planet_transform;
    planet->UpdateVisuals(world);

    world.GetComponent<svanes::Transform>(rotating_square->Get()).rotation =
        update.square_rotation;

    // Collect the existing attachment trees into one list so their objects can
    // be kept even if they change parents or become independent in this update.
    for (std::size_t index = 0; index < root_objects.size(); ++index) {
        auto attachments = root_objects[index]->ReleaseAttachments();
        for (auto &attachment : attachments) {
            root_objects.push_back(std::move(attachment));
        }
    }

    // The owners of existing and newly created objects, keyed by their local
    // entity IDs until ownership is transferred to the new attachment trees.
    // Predicted-only objects have no entry in objects, so they stay here and
    // are destroyed when the authoritative update finishes.
    std::unordered_map<svanes::Entity, std::unique_ptr<DynamicObject>> owned;
    for (auto &object : root_objects) {
        const auto entity = object->GetEntity();
        world.RemoveComponent<Attachment>(entity);
        owned.emplace(entity, std::move(object));
    }
    root_objects.clear();

    // The server IDs of objects present in this complete world update.
    std::unordered_set<svanes::Entity> present;

    // For every object in the server's world, create it if necessary and update
    // its displayed state. The server's world update is authoritative, so we
    // copy the server's state to the client.
    for (const auto &state : update.objects) {
        present.insert(state.entity);

        // Create the object if it does not already exist in the client's world.
        if (!objects.contains(state.entity)) {
            auto object = assets.CreateUnattached(
                world, gameplay_timeline->Get(), state.type, state.definition);
            objects.emplace(state.entity, object.get());
            owned.emplace(object->GetEntity(), std::move(object));
        }

        // And now at this point we know the object exists in the client's
        // world, so we can update its displayed state to match the server's
        // world update.

        // The local object corresponding to this server entity.
        auto &object = *objects.at(state.entity);
        const auto entity = object.GetEntity();

        world.GetComponent<svanes::Timeline>(entity).SetProgress(
            state.local_tics, 0, svanes::RationalNumber{0});

        // Copy independent movement if present. Static and attached objects
        // have no kinematic component.
        if (state.velocity) {
            auto &motion = world.AddComponent<svanes::Kinematic2D>(entity);
            motion.velocity_x = state.velocity->linear.x;
            motion.velocity_y = state.velocity->linear.y;
            motion.angular_velocity = state.velocity->angular;
        } else {
            world.RemoveComponent<svanes::Kinematic2D>(entity);
        }

        // Keep damageable objects' health in step with the server.
        if (state.health) {
            world.AddComponent<Health>(entity, Health{*state.health});
        } else {
            world.RemoveComponent<Health>(entity);
        }

        // Apply any per-object color instead of the catalog's default color.
        if (state.color) {
            world.AddComponent<svanes::Color>(entity, *state.color);
        } else {
            world.RemoveComponent<svanes::Color>(entity);
        }

        // Launch history is estimated locally rather than sent by the server.
        if (world.HasComponent<MissileExplosion>(entity)) {
            auto &explosion = world.GetComponent<MissileExplosion>(entity);
            explosion.firing_ship.reset();
            explosion.launched_at = state.local_tics;
            explosion.detonated = false;
        }
    }

    // For every server object, if it is a missile that is no longer present,
    // it must have blown up. so we need to create a flash for it.
    // Assumes that missiles can only be destroyed by detonating.
    for (const auto &[id, object] : objects) {
        if (!present.contains(id) &&
            object->GetType() == DynamicObjectType::Missile) {
            const auto &pose = object->GetTransform(world);
            CreateCollisionFlash(world, effects_timeline->Get(),
                                 collision_flashes, {pose.x, pose.y});
        }
    }

    // Remove lookup entries for objects which are no longer in the server's
    // world. Their owners remain in owned and destroy them when it goes out of
    // scope.
    std::erase_if(objects, [&](const auto &entry) {
        return !present.contains(entry.first);
    });

    // Rebuild the owning attachment vectors and registry relationships using
    // the server's parent IDs. Objects without parents remain independently
    // owned by the client.
    for (const auto &state : update.objects) {
        auto &object = *objects.at(state.entity);
        const auto entity = object.GetEntity();
        if (state.attachment) {
            objects.at(state.attachment->parent)
                ->AddAttachment(world, std::move(owned.at(entity)),
                                state.attachment->transform);
        } else {
            root_objects.push_back(std::move(owned.at(entity)));
        }
    }

    // Attaching objects recalculates their positions. Restore the server's
    // exact world poses now that the complete attachment trees exist.
    for (const auto &state : update.objects) {
        objects.at(state.entity)->GetTransform(world) = state.transform;
    }

    // Estimate launcher readiness from the ammunition actually attached in
    // this snapshot. An empty slot starts a fresh local reload countdown.
    for (const auto &state : update.objects) {
        const auto &object = *objects.at(state.entity);
        const auto entity = object.GetEntity();
        if (world.HasComponent<MissileLauncherState>(entity)) {
            auto &launcher = world.GetComponent<MissileLauncherState>(entity);
            launcher.loaded_missile.reset();
            launcher.emptied_at = state.local_tics;
            launcher.ammunition_lost = false;
            for (const auto &child : object.GetAttachments()) {
                if (child->GetType() == DynamicObjectType::Missile) {
                    launcher.loaded_missile = child->GetEntity();
                    launcher.emptied_at.reset();
                    break;
                }
            }
            world.GetComponent<WeaponControl>(entity).fire = false;
        }
    }

    // Update the complete trees' visuals from their roots.
    for (const auto &object : root_objects) {
        object->UpdateVisuals(world);
    }

    // Follow this player's living ship. If it has been destroyed, keep the
    // camera at its last position.
    player_ship.reset();
    for (const auto &player : update.players) {
        if (player.client == client_id) {
            player_ship = player.ship;

            if (player_ship) {
                const auto &pose =
                    objects.at(*player_ship)->GetTransform(world);
                frame.camera.x = pose.x;
                frame.camera.y = pose.y;
            }
        }
    }
}

void OrbitalEscalationClient::SendInput(
    const svanes::ClientFrameContext &frame) {
    if (!frame.network.IsConnected()) {
        return;
    }

    // The player's requested thrust and rotation, neutral without a ship.
    PropulsionControl control{};

    // Whether the player requested a shot during this frame.
    bool fire = false;

    // If the player has a ship, read its movement and firing controls.
    if (player_ship) {
        const auto entity = objects.at(*player_ship)->GetEntity();
        control = ReadShipControls(
            frame.input, frame.camera,
            frame.world.GetComponent<svanes::Transform>(entity),
            frame.world.GetComponent<svanes::Kinematic2D>(entity),
            frame.world.GetComponent<Propulsion>(entity));
        fire = frame.input.WasMouseButtonPressed(svanes::MouseButton::Right);

        frame.world.GetComponent<PropulsionControl>(entity) = control;
        // Keep a firing request until a physics step can consume it.
        if (fire) {
            SetWeaponControls(frame.world, *objects.at(*player_ship), {true});
        }
    }

    // The writer collecting this frame's gameplay controls.
    svanes::MessageWriter writer;

    // An input message consists of the following fields, in order:
    // - 4 bytes: horizontal thrust request, as a 32-bit float
    // - 4 bytes: vertical thrust request, as a 32-bit float
    // - 4 bytes: rotation request, as a 32-bit float
    // - 1 byte: firing request, zero or one
    // - 1 byte: pause toggle request, zero or one

    writer.WriteFloat32(control.thrust.x);
    writer.WriteFloat32(control.thrust.y);
    writer.WriteFloat32(control.rotation);
    writer.WriteBool(fire);
    writer.WriteBool(frame.input.WasPressed(svanes::Key::P));
    frame.network.Send(writer.Finish());
}

void OrbitalEscalationClient::Predict(svanes::Registry &world,
                                      svanes::EventManager &events) {
    svanes::AdvancePhysics(
        world, {}, parallel_for, [&](std::span<const svanes::PhysicsTimeStep>) {
            ApplyPropulsion(world);
            UpdateAttachments(world);

            // Clientside collision detection

            std::vector<svanes::Entity> colliders;
            world.ForEach<svanes::Collider2D, svanes::Transform>(
                [&](svanes::Entity entity, const auto &, const auto &) {
                    if (!IsDead(world, entity)) {
                        colliders.push_back(entity);
                    }
                });
            auto contacts = svanes::DetectEntityCollisions(
                world, colliders, events, parallel_for);

            // Attached parts do not collide with each other, and freshly fired
            // missiles ignore their firing ship while arming.
            std::erase_if(contacts, [&](const auto &pair) {
                return GetAttachmentRoot(world, pair.a) ==
                           GetAttachmentRoot(world, pair.b) ||
                       IgnoresFiringShip(world, pair.a, pair.b) ||
                       IgnoresFiringShip(world, pair.b, pair.a);
            });

            // Clientside collision response

            for (const auto &pair : contacts) {
                ApplyCollisionAcceleration(world, pair);
            }

            // The ships touching the planet in this step.
            std::unordered_set<svanes::Entity> contacting;

            // For every ship touching the planet, apply impact damage once per
            // contact.
            for (const auto &[id, object] : objects) {
                const auto entity = object->GetEntity();

                // Only ships can take impact damage, and only if they are
                // alive.
                if (object->GetType() != DynamicObjectType::Ship ||
                    IsDead(world, entity)) {
                    continue;
                }

                // Check if this ship is touching the planet. If so, add it to
                // the set of ships touching the planet this step.
                for (const auto &pair : contacts) {
                    const auto surface = planet->GetEntity();
                    if ((pair.a == entity && pair.b == surface) ||
                        (pair.b == entity && pair.a == surface)) {
                        contacting.insert(entity);
                    }
                }

                // If this ship is touching the planet now but was not touching
                // it in the previous step, apply impact damage once.
                if (contacting.contains(entity) &&
                    !touching_planet.contains(entity)) {
                    ApplyDamage(world, entity, kPlanetImpactDamage);
                }
            }
            touching_planet = std::move(contacting);

            // Create explosion flashes for missiles that detonated this step.
            for (const auto position :
                 UpdateMissileExplosions(world, contacts)) {
                CreateCollisionFlash(world, effects_timeline->Get(),
                                     collision_flashes, position);
            }

            // Predict the results of collisions with the deletion boxes.
            for (const auto position :
                 ApplyDeletionBoxes(world, deletion_boxes)) {
                CreateCollisionFlash(world, effects_timeline->Get(),
                                     collision_flashes, position);
            }

            // Remove dead objects from the world, including their attachments.
            // If a parent is dead, its children are also dead, so we can remove
            // the entire attachment tree at once. If a child is dead, we remove
            // it from its parent's attachment vector, but the parent remains
            // alive.
            std::erase_if(objects, [&](const auto &entry) {
                auto entity = entry.second->GetEntity();
                while (true) {
                    if (IsDead(world, entity)) {
                        return true;
                    }

                    if (!world.HasComponent<Attachment>(entity)) {
                        return false;
                    }

                    entity = world.GetComponent<Attachment>(entity).parent;
                }
            });

            if (player_ship && !objects.contains(*player_ship)) {
                player_ship.reset();
            }

            std::erase_if(root_objects, [&](const auto &object) {
                if (IsDead(world, object->GetEntity())) {
                    return true;
                }

                object->RemoveDeadAttachments(world);
                return false;
            });

            // Updating weapons can append fired missiles. Visit the roots
            // which existed at the start so new missiles wait until next time.
            const auto count = root_objects.size();
            for (std::size_t index = 0; index < count; ++index) {
                UpdateWeapons(world, *root_objects[index], assets,
                              gameplay_timeline->Get(), root_objects);
            }

            UpdateAttachments(world);
            for (const auto &object : root_objects) {
                object->UpdateVisuals(world);
            }
        });
}

bool OrbitalEscalationClient::ShouldQuit() const { return should_quit; }
