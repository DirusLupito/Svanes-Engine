#pragma once

#include <svanes/network/network_session.hpp>
#include <svanes/network/peer_group.hpp>
#include <svanes/network/udp_msg_pipe.hpp>

#include <chrono>
#include <optional>
#include <string>

namespace svanes::internal {

/**
 * The joining side of admission, used by PeerGroup before it has an id. Asks
 * one member of an existing world to admit this process through contact
 * packets, repeated until the member answers with an assignment or a
 * rejection, and gives up after 15 seconds.
 */
class PeerJoin {
public:
    /**
     * Prepares to contact a member.
     * @param session The session, not yet admitted, that carries the requests.
     * @param pipe The pipe owned by that session.
     * @param entry The member's host and port.
     * @param rules_hash The rules hash the member checks before admitting.
     */
    PeerJoin(NetworkSession &session, UdpMsgPipe &pipe, UdpAddress entry,
             std::uint64_t rules_hash);

    /**
     * Resends the request when due and reads replies. Call after the
     * session's Update().
     * @throws std::invalid_argument for a malformed reply.
     */
    void Update();

    /** @return The assignment, once one has arrived. */
    const std::optional<PeerAssignment> &Assignment() const;

    /** @return The connection to the contacted member. */
    ConnectionId EntryConnection() const;

    /** @return Why joining failed, or empty while it has not. */
    const std::string &Failure() const;

    /** @return A short status for the console. */
    std::string Status() const;

private:
    NetworkSession &session;
    UdpAddress entry;
    ConnectionId entry_connection;
    std::uint64_t rules_hash;
    std::chrono::steady_clock::time_point started_at;
    std::optional<std::chrono::steady_clock::time_point> last_request;
    bool answered = false;
    std::optional<PeerAssignment> assignment;
    std::string failure;
};

} // namespace svanes::internal
