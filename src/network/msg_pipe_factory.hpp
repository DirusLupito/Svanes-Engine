#pragma once

#include <svanes/network/tcp_msg_pipe.hpp>
#include <svanes/network/udp_msg_pipe.hpp>

#include <memory>

namespace svanes::internal {

/**
 * Creates a new message pipe instance. TODO: Make this configurable.
 * The client-server netcode is hardcoded to use UDP for message transport.
 * In the future, this function can be easily changed to create some other type
 * of message pipe, such as TCP, without requiring changes to the rest of the
 * netcode.
 *
 * @return A unique pointer to a new MsgPipe instance.
 */
inline std::unique_ptr<MsgPipe> MakePipe() {
    return std::make_unique<UdpMsgPipe>();
}

} // namespace svanes::internal
