#include "ServerPacketReceivedHandler.hpp"

#include "../ClientRole.hpp"
#include "../events/EndTurnEvent.hpp"
#include "../events/MovementEvent.hpp"
#include "../events/PlacementEvent.hpp"

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

              bool includeSender = false;
              try {
                  const auto activeTeam = board.GetActiveTeam();
                  if (GetClientTeam(event.client) != activeTeam) {
                      throw std::invalid_argument("It is not your turn.");
                  }
                  switch (GetGameMessageType(event.message)) {
                  case GameMessageType::EndTurn:
                      events.TriggerEvent(
                          EndTurnEvent::FromMessage(event.message));
                      includeSender = true;
                      break;
                  case GameMessageType::Placement: {
                      const auto placement =
                          PlacementEvent::FromMessage(event.message);
                      if (placement.piece.team != activeTeam) {
                          throw std::invalid_argument(
                              "That reserve belongs to the other team.");
                      }
                      events.TriggerEvent(placement);
                      break;
                  }
                  case GameMessageType::Movement: {
                      const auto movement =
                          MovementEvent::FromMessage(event.message);
                      const auto &piece = board.GetPiece(movement.from);
                      if (piece && piece->team != activeTeam) {
                          throw std::invalid_argument(
                              "That piece belongs to the other team.");
                      }
                      events.TriggerEvent(movement);
                      break;
                  }
                  case GameMessageType::BoardState:
                      throw std::invalid_argument(
                          "Only the GHQ server can supply a board state.");
                  }
              } catch (const std::invalid_argument &error) {
                  std::cout << "Rejected GHQCXX request from client "
                            << event.client << ": " << error.what() << '\n'
                            << std::flush;
                  network.Send(event.client, board.ToMessage());
                  return;
              } catch (const std::out_of_range &error) {
                  std::cout << "Rejected GHQCXX request from client "
                            << event.client << ": " << error.what() << '\n'
                            << std::flush;
                  network.Send(event.client, board.ToMessage());
                  return;
              }

              // The sender predicts moves and placements. All clients advance
              // the turn when they receive the server's end-turn event.
              for (const auto client : clients) {
                  if (includeSender || client != event.client) {
                      network.Send(client, event.message);
                  }
              }
          }) {}

} // namespace ghqcxx
