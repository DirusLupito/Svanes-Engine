#pragma once

#include <svanes/network/network_message.hpp>

namespace svanes {

/**
 *
 */
class MsgPipe {
public:
    /**
     *
     */
    virtual ~MsgPipe() = default;

    /**
     *
     */
    virtual bool Send(const NetworkMessage &message) = 0;

    /**
     *
     */
    virtual bool Receive(NetworkMessage &message) = 0;
};

} // namespace svanes
