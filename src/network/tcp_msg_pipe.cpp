#include <svanes/network/tcp_msg_pipe.hpp>

#include <stdexcept>
#include <utility>
#include <vector>

namespace svanes {

void TcpMsgPipe::Listen(const std::string &address) {
    if (socket) {
        throw std::logic_error(
            "TcpMsgPipe: close the pipe before configuring it again.");
    }

    // PULL sockets can only receive, which is exactly the receiving end of
    // our directed edge. They also fair-queue between every PUSH socket
    // connected to them, so many senders can share one listener.
    zmq::socket_t receiver(context, zmq::socket_type::pull);

    // When this socket closes, discard any messages still waiting around.
    receiver.set(zmq::sockopt::linger, 0);

    // Bind the socket to the specified address to establish the receiving
    // direction.
    receiver.bind("tcp://" + address);

    // Now the pipe owns the socket and is ready to receive messages.
    socket = std::move(receiver);
    listening = true;
}

void TcpMsgPipe::Connect(const std::string &address) {
    if (socket) {
        throw std::logic_error(
            "TcpMsgPipe: close the pipe before configuring it again.");
    }

    // PUSH sockets can only send, so this is the sending end of the edge.
    zmq::socket_t sender(context, zmq::socket_type::push);

    // When this socket closes, discard any messages still waiting to be sent.
    sender.set(zmq::sockopt::linger, 0);

    // Unlike DGRAM, TCP sockets support connect, so we don't need to parse
    // and store the destination ourselves. 0mq validates the address for us
    // and makes the actual connection in the background (so this will
    // succeed even if nobody is listening yet).
    sender.connect("tcp://" + address);

    // This message pipe owns the socket now.
    socket = std::move(sender);
    listening = false;
}

std::uint16_t TcpMsgPipe::Port() const {
    if (!socket) {
        throw std::logic_error(
            "TcpMsgPipe: Port requires a listening or connected pipe.");
    }

    // The last endpoint looks like tcp://HOST:PORT. If we listened on port 0,
    // this has the port the OS actually picked, not 0.
    const auto endpoint = socket.get(zmq::sockopt::last_endpoint);

    // So everything after the last colon is the port number.
    return static_cast<std::uint16_t>(
        std::stoul(endpoint.substr(endpoint.rfind(':') + 1)));
}

bool TcpMsgPipe::Send(const NetworkMessage &message) {
    if (!socket || listening) {
        throw std::logic_error(
            "TcpMsgPipe: Send requires a connected sending pipe.");
    }

    // TCP is a stream, but 0mq handles framing for us, so the whole message
    // goes out as a single frame and arrives as a single frame. We send
    // without blocking (dontwait). If the socket's internal buffers are full,
    // or the connection hasn't been made yet, this will return false.
    return socket
        .send(zmq::buffer(message.bytes.data(), message.bytes.size()),
              zmq::send_flags::dontwait)
        .has_value();
}

bool TcpMsgPipe::Receive(NetworkMessage &message) {
    if (!socket || !listening) {
        throw std::logic_error(
            "TcpMsgPipe: Receive requires a listening receiving pipe.");
    }

    zmq::message_t body;

    // We delegate to the socket's recv method, which will return an empty
    // optional if no message is available. dontwait so we don't block if no
    // message is available.
    if (!socket.recv(body, zmq::recv_flags::dontwait).has_value()) {
        return false;
    }

    // There's no address frame like with UDP, so the one frame we got is the
    // whole message. Copy the received bytes into the provided NetworkMessage
    // object.

    const auto *bytes = body.data<std::byte>();
    message =
        NetworkMessage{std::vector<std::byte>(bytes, bytes + body.size())};

    return true;
}

void TcpMsgPipe::Close() {
    socket.close();
    listening = false;
}

} // namespace svanes
