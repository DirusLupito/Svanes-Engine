#pragma once

#include "Board.hpp"

#include <svanes/network/clientserver/IServer.hpp>

namespace ghqcxx {

/**
 * Represents the GHQ game hosted by the server. Owns its board and pieces
 * and handles clients joining and leaving the game.
 */
class GHQCXXServer final : public svanes::IServer {
public:
    /**
     * Sets up the server's board and prints its startup message.
     *
     * @param context The engine services and configuration provided for
     * server initialization.
     */
    void Initialize(svanes::ServerContext &context) override;

    /**
     * Accepts the client and reports its assigned ID.
     *
     * @param context The client's request and reserved ID.
     *
     * @return An acceptance with no additional game data.
     */
    svanes::JoinResponse
    HandleJoinRequest(const svanes::ServerJoinContext &context) override;

    /**
     * Reports a client that has disconnected or timed out.
     *
     * @param world The server's registry.
     * @param client The ID of the disconnected client.
     */
    void HandleDisconnect(svanes::Registry &world,
                          svanes::ClientId client) override;

    /**
     * Handles gameplay messages received from clients.
     *
     * @param context The server's registry, networking, and gravity for this
     * update.
     */
    void Update(const svanes::ServerUpdateContext &context) override;

private:
    // The engine's event manager, supplied during initialization.
    svanes::EventManager *events = nullptr;

    // The server's board state.
    Board board;
};

} // namespace ghqcxx
