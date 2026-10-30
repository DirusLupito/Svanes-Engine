#include "orbital_escalation_client.hpp"
#include "collision_response.hpp"
#include "combat/damage_system.hpp"
#include "combat/missile_system.hpp"
#include "combat/weapon_system.hpp"
#include "controls.hpp"
#include "effects/collision_flashes.hpp"
#include "game_objects/deletion_boxes.hpp"
#include "game_objects/world_obstacles.hpp"
#include "player_events.hpp"
#include "world_snapshot_event.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/collision_pass.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/network/clientserver/network_client.hpp>
#include <svanes/physics_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <utility>

const svanes::TicCount kPauseFlashPeriod = svanes::SecondsToTics(1.0);
constexpr std::uint8_t kPauseLabelMinimumAlpha = 64;
constexpr svanes::TicCount kClientFrameTics = 5000; // 1/200 second

OrbitalEscalationClient::OrbitalEscalationClient(std::string server_address)
    : server_address(std::move(server_address)),
      assets(svanes::AssetPath("assets/orbitalEscalation")) {
    if (this->server_address.empty()) {
        throw std::invalid_argument("The server address must be provided.");
    }
}

void OrbitalEscalationClient::Initialize(svanes::ClientContext &context) {
    RegisterPhysicalCollisionHandler(context.world, context.events);
    planet_collision_damage.Register(context.world, context.events);
    missile_collisions.Register(context.world, context.events);
    deletion_box_collisions.Register(
        context.world, context.events,
        [this, &world = context.world](svanes::Vector2D position) {
            CreateCollisionFlash(world, effects_timeline->Get(),
                                 collision_flashes, position);
        });
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

    snapshot_event_handlers.emplace(ClientEventContext{
        .world = context.world,
        .events = context.events,
        .camera = context.camera,
        .assets = assets,
        .client_id = client_id,
        .gameplay_timeline = gameplay_timeline->Get(),
        .effects_timeline = effects_timeline->Get(),
        .rotating_square = rotating_square->Get(),
        .pause_label = pause_label_entity,
        .planet = planet,
        .player_ship = player_ship,
        .objects = objects,
        .root_objects = root_objects,
        .collision_flashes = collision_flashes,
    });

    context.events.Subscribe(
        PlayerSpawnEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<PlayerSpawnEvent>>(
            [this](const PlayerSpawnEvent &event) {
                snapshot_event_handlers->HandlePlayerSpawn(event);
            }));

    context.events.Subscribe(
        PlayerDeathEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<PlayerDeathEvent>>(
            [this](const PlayerDeathEvent &event) {
                snapshot_event_handlers->HandlePlayerDeath(event);
            }));

    context.events.Subscribe(
        WorldSnapshotEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<WorldSnapshotEvent>>(
            [this](const WorldSnapshotEvent &event) {
                snapshot_event_handlers->HandleWorldSnapshot(event);
            }));
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
        frame.events.QueueEvent(std::make_unique<WorldSnapshotEvent>(
            WorldUpdate::Read(messages.back())));
        frame.events.DispatchEvents();
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
            planet_collision_damage.BeginStep(planet->GetEntity());
            svanes::DetectEntityCollisions(world, colliders, events,
                                           parallel_for);

            // Clientside collision response

            events.DispatchEvents();

            // Create explosion flashes for missiles that detonated this step.
            for (const auto position :
                 missile_collisions.UpdateExplosions(world)) {
                CreateCollisionFlash(world, effects_timeline->Get(),
                                     collision_flashes, position);
            }

            // Player ships remain until a server snapshot removes them. Other
            // dead objects take their attachments with them, so remove lookup
            // entries for the complete tree before destroying its owner.
            std::erase_if(objects, [&](const auto &entry) {
                auto entity = entry.second->GetEntity();
                while (true) {
                    if (IsDead(world, entity) &&
                        !snapshot_event_handlers->IsPlayerShip(entity)) {
                        return true;
                    }

                    if (!world.HasComponent<Attachment>(entity)) {
                        return false;
                    }

                    entity = world.GetComponent<Attachment>(entity).parent;
                }
            });

            std::erase_if(root_objects, [&](const auto &object) {
                if (IsDead(world, object->GetEntity()) &&
                    !snapshot_event_handlers->IsPlayerShip(object->GetEntity())) {
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
