#include "orbital_escalation_client.hpp"
#include "combat/damage_system.hpp"
#include "controls.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/network/clientserver/network_client.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <unordered_set>
#include <utility>

const svanes::TicCount kPauseFlashPeriod = svanes::SecondsToTics(1.0);
constexpr std::uint8_t kPauseLabelMinimumAlpha = 64;
constexpr svanes::TicCount kClientFrameTics = 16666;

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

svanes::TicCount OrbitalEscalationClient::GetFrameIntervalTics() const {
    const auto interval =
        svanes::RationalNumber{kClientFrameTics} /
        (svanes::RationalNumber{1} / client_loop_timeline.GetTicSize());

    return interval.GetWholePart() +
           (interval.GetFractionalPart().GetNumerator() != 0);
}

void OrbitalEscalationClient::Update(const svanes::ClientFrameContext &frame) {
    // The world update messages received for this frame.
    const auto messages = frame.network.PollMessages();

    // Each update describes the complete world state, so only apply the last
    // one before the engine renders this frame.
    if (!messages.empty()) {
        ApplyWorldUpdate(frame, WorldUpdate::Read(messages.back()));
    }

    if (frame.input.WasPressed(svanes::Key::Escape)) {
        should_quit = true;
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

    // 1.1^delta
    // Rolling harder on the mouse wheel will zoom in and out
    // faster compared to rolling the same distance slowly.
    const float zoom = std::clamp(
        frame.camera.zoom * std::pow(1.1F, frame.input.MouseWheelThisFrame().y),
        0.01F, 100.0F);
    frame.camera.zoom = zoom;

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

    SendInput(frame);
}

void OrbitalEscalationClient::ApplyWorldUpdate(
    const svanes::ClientFrameContext &frame, const WorldUpdate &update) {
    // The registry containing the client's displayed objects and local UI.
    auto &world = frame.world;

    // The master clock shared by the displayed gameplay objects.
    auto &timeline =
        world.GetComponent<svanes::Timeline>(gameplay_timeline->Get());

    // Copy the server's clock state. The client does not advance the simulation
    // between updates. (TODO: change this? for client-side prediction?)
    timeline.SetProgress(update.total_tics, 0, svanes::RationalNumber{0});
    timeline.SetTicSize(update.tic_size);
    if (update.paused) {
        timeline.Pause();
    } else {
        timeline.Unpause();
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

    // The server IDs of objects present in this complete world update.
    std::unordered_set<svanes::Entity> present;

    // For every object in the server's world, create it if necessary and update
    // its displayed state. The server's world update is authoritative, so we
    // copy the server's state to the client.
    for (const auto &state : update.objects) {
        present.insert(state.entity);

        // Create the object if it does not already exist in the client's world.
        if (!objects.contains(state.entity)) {
            objects.emplace(state.entity, assets.CreateUnattached(
                                              world, gameplay_timeline->Get(),
                                              state.type, state.definition));
        }

        // And now at this point we know the object exists in the client's
        // world, so we can update its displayed state to match the server's
        // world update.

        // The local object corresponding to this server entity.
        auto &object = *objects.at(state.entity);
        const auto entity = object.GetEntity();

        object.GetTransform(world) = state.transform;
        world.GetComponent<svanes::Timeline>(entity).SetProgress(
            state.local_tics, 0, svanes::RationalNumber{0});

        // Copy independent movement if present. Attached objects have no
        // kinematic component while they follow their parent.
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
    }

    // Remove objects which are no longer in the server's world. Their owners
    // also remove the associated visual entities from the registry.
    std::erase_if(objects, [&](const auto &entry) {
        return !present.contains(entry.first);
    });

    // All objects now exist, so translate attachment parent IDs to local
    // entities. Their world poses already came from the server.
    for (const auto &state : update.objects) {
        auto &object = *objects.at(state.entity);
        const auto entity = object.GetEntity();
        if (state.attachment) {
            world.AddComponent<Attachment>(
                entity,
                Attachment{objects.at(state.attachment->parent)->GetEntity(),
                           state.attachment->transform});
        } else {
            world.RemoveComponent<Attachment>(entity);
        }
        object.UpdateVisuals(world);
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

bool OrbitalEscalationClient::ShouldQuit() const { return should_quit; }
