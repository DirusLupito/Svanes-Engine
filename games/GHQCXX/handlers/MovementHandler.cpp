#include "MovementHandler.hpp"

#include <iostream>

namespace ghqcxx {

MovementHandler::MovementHandler()
    : svanes::EventHandlerWrapper<MovementEvent>(
          [](const MovementEvent &event) {
              std::cout << "GHQCXX received " << event.GetEventType() << ".\n"
                        << std::flush;
          }) {}

} // namespace ghqcxx
