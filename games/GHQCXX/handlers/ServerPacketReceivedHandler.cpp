#include "ServerPacketReceivedHandler.hpp"

#include "../events/MovementEvent.hpp"

#include <svanes/events/event_manager.hpp>

#include <iostream>
#include <stdexcept>

namespace ghqcxx {

ServerPacketReceivedHandler::ServerPacketReceivedHandler(
    svanes::EventManager &events, svanes::NetworkServer &network,
    const std::vector<svanes::ClientId> &clients, const Board &board)
    : svanes::EventHandlerWrapper<ServerPacketReceivedEvent>(
          [&events, &network, &clients,
           &board](const ServerPacketReceivedEvent &event) {
              std::cout
                  << "GHQCXX server received a gameplay packet from client "
                  << event.client << " (" << event.message.bytes.size()
                  << " bytes).\n"
                  << std::flush;

              try {
                  events.TriggerEvent(
                      MovementEvent::FromMessage(event.message));
              } catch (const std::invalid_argument &error) {
                  std::cout << "Rejected GHQCXX move from client "
                            << event.client << ": " << error.what() << '\n'
                            << std::flush;
                  network.Send(event.client, board.ToMessage());
                  return;
              } catch (const std::out_of_range &error) {
                  std::cout << "Rejected GHQCXX move from client "
                            << event.client << ": " << error.what() << '\n'
                            << std::flush;
                  network.Send(event.client, board.ToMessage());
                  return;
              }

              // Relay only after the move succeeds. Its sender has already
              // applied it locally, so sending it back would move it twice.
              for (const auto client : clients) {
                  if (client != event.client) {
                      network.Send(client, event.message);
                  }
              }
          }) {}

} // namespace ghqcxx
