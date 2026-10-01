#include "OrbitalEscalationServer.hpp"
#include <iostream>
#include <svanes/network/server_runtime.hpp>

OrbitalEscalationServer::OrbitalEscalationServer(
    const std::filesystem::path &assets, std::uint16_t joining_port)
    : simulation(world, assets), network(joining_port) {
    std::cout << "Orbital Escalation server: TCP joining port " << joining_port
              << ".\n";
}

void OrbitalEscalationServer::Receive(const svanes::ServerMessage &message) {
    svanes::MessageReader reader(message.message);
    const auto input = simulation.DecodeInput(reader);
    const auto player = static_cast<std::uint32_t>(message.session + 1);
    auto [entry, added] = inputs.try_emplace(player);
    if (added) {
        simulation.AddPlayer({player});
        std::cout << "Player " << player << " joined.\n";
    }
    // Several messages may arrive before a step. Keep firing and pause presses
    // until the step consumes them.
    entry->second.propulsion = input.propulsion;
    entry->second.weapons.fire |= input.weapons.fire;
    entry->second.pause |= input.pause;
}

void OrbitalEscalationServer::Step() {
    std::vector<OrbitalInput> controls;
    // Held controls persist, while each firing or pause request is used once.
    for (auto &[player, input] : inputs) {
        controls.push_back(input);
        input.weapons.fire = false;
        input.pause = false;
    }

    simulation.Step(controls);
    snapshot_tics += OrbitalStepTics;
    if (snapshot_tics >= OrbitalSnapshotTics) {
        snapshot_tics = 0;
        const auto snapshot = simulation.Save();
        for (const auto &[player, input] : inputs) {
            network.Send(player - 1, snapshot);
        }
    }
}

void OrbitalEscalationServer::Run() {
    svanes::ServerRuntime::Run(
        network, OrbitalStepTics,
        [this](const auto &message) { Receive(message); }, [this] { Step(); });
}
