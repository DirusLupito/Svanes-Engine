#include "ghqcxx_server.hpp"

#include "events/ServerPacketReceivedEvent.hpp"
#include "handlers/EndTurnHandler.hpp"
#include "handlers/MovementHandler.hpp"
#include "handlers/PlacementHandler.hpp"
#include "handlers/ServerPacketReceivedHandler.hpp"

#include <svanes/events/event_manager.hpp>

#include <iostream>
#include <memory>

namespace ghqcxx {

void GHQCXXServer::Initialize(svanes::ServerContext &context) {
    events = &context.events;
    board.Initialize();

    context.events.Subscribe(std::make_unique<ServerPacketReceivedHandler>(
        context.events, context.network, clients, board));
    context.events.Subscribe(std::make_unique<MovementHandler>(board));
    context.events.Subscribe(std::make_unique<PlacementHandler>(board));
    context.events.Subscribe(std::make_unique<EndTurnHandler>(board));

    std::cout << "Hello, world! GHQCXX server.\n" << std::flush;
}

svanes::JoinResponse
GHQCXXServer::HandleJoinRequest(const svanes::ServerJoinContext &context) {
    clients.push_back(context.client);
    std::cout << "Accepting GHQCXX client " << context.client << ".\n"
              << std::flush;
    return {.accepted = true, .message = board.ToMessage()};
}

void GHQCXXServer::HandleDisconnect(svanes::Registry &,
                                    svanes::ClientId client) {
    std::erase(clients, client);
    std::cout << "GHQCXX client " << client << " disconnected.\n" << std::flush;
}

void GHQCXXServer::Update(const svanes::ServerUpdateContext &context) {
    for (const auto &input : context.network.PollInputs()) {
        events->TriggerEvent(
            ServerPacketReceivedEvent(input.client, input.message));
    }
}

} // namespace ghqcxx
