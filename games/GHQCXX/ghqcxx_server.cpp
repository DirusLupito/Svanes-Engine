#include "ghqcxx_server.hpp"

#include "events/ServerPacketReceivedEvent.hpp"

#include <svanes/events/event_manager.hpp>

#include <iostream>

namespace ghqcxx {

void GHQCXXServer::Initialize(svanes::ServerContext &context) {
    events = &context.events;
    board.Initialize();
    std::cout << "Hello, world! GHQCXX server.\n" << std::flush;
}

svanes::JoinResponse
GHQCXXServer::HandleJoinRequest(const svanes::ServerJoinContext &context) {
    std::cout << "Accepting GHQCXX client " << context.client << ".\n"
              << std::flush;
    return {.accepted = true, .message = {}};
}

void GHQCXXServer::HandleDisconnect(svanes::Registry &,
                                    svanes::ClientId client) {
    std::cout << "GHQCXX client " << client << " disconnected.\n" << std::flush;
}

void GHQCXXServer::Update(const svanes::ServerUpdateContext &context) {
    for (const auto &input : context.network.PollInputs()) {
        events->TriggerEvent(
            ServerPacketReceivedEvent(input.client, input.message));
    }
}

} // namespace ghqcxx
